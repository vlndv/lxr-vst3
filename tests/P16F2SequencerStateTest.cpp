#include "../vendor/iPlug2/Examples/LXR/LXRProjectState.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
unsigned passed = 0, failed = 0;
void check(bool v, int line)
{
    if (v) ++passed;
    else { ++failed; std::fprintf(stderr, "FAIL line %d\n", line); }
}
#define CHECK(v) check((v), __LINE__)
using namespace lxr;
using namespace lxr::state;
bool equal(const sequencer::PatternBank& a, const sequencer::PatternBank& b)
{
    sequencer::state::EncodedBank aa, bb;
    return sequencer::state::Encode(a, aa) && sequencer::state::Encode(b, bb) && aa == bb;
}
sequencer::state::EncodedBank read(const char* dir, const char* name)
{
    char path[2048]; std::snprintf(path, sizeof(path), "%s/%s.lxqb", dir, name);
    sequencer::state::EncodedBank data {};
    auto* f = std::fopen(path, "rb");
    if (f == nullptr) { CHECK(false); return data; }
    CHECK(std::fread(data.data(), 1, data.size(), f) == data.size());
    CHECK(std::fgetc(f) == EOF); std::fclose(f);
    return data;
}
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    using namespace lxr;
    using namespace lxr::state;
    auto defaults = sequencer::MakeDefaultPatternBank();
    auto defaultBytes = read(argv[1], "default");
    auto denseBytes = read(argv[1], "dense");
    auto musicalBytes = read(argv[1], "musical");
    sequencer::state::EncodedBank encoded {};
    CHECK(sequencer::state::Encode(defaults, encoded)); CHECK(encoded == defaultBytes);
    sequencer::PatternBank dense;
    CHECK(sequencer::state::Decode(denseBytes.data(), denseBytes.size(), dense));
    for (std::size_t p = 0; p < 8; ++p)
    {
        for (std::size_t t = 0; t < 7; ++t)
        {
            const auto k = p * 7 + t;
            CHECK(dense.patterns[p].mainGates[t] == ((k * 1009) & 65535));
            CHECK(dense.patterns[p].lengthCodes[t] == k % 16);
            CHECK(dense.patterns[p].rotations[t] == (k * 3) % 16);
            for (std::size_t s = 0; s < 128; ++s)
            {
                const auto i = k * 128 + s; const auto& step = dense.patterns[p].steps[t][s];
                CHECK(step.volume == i % 256); CHECK(step.probability == i % 128);
                CHECK(step.note == (i * 3) % 128);
                CHECK(step.parameter1 == (i % 3 == 0 ? 255 : i % 228));
                CHECK(step.value1 == (i * 5) % 128);
                CHECK(step.parameter2 == (i % 5 == 0 ? 255 : (i * 7) % 228));
                CHECK(step.value2 == (i * 11) % 128);
            }
        }
        CHECK(dense.patterns[p].nextPattern == (p * 2) % 15);
        CHECK(dense.patterns[p].repeat == p * 13);
    }
    CHECK(dense.shuffle == 87);
    CHECK(sequencer::state::Encode(dense, encoded)); CHECK(encoded == denseBytes);
    CHECK(sequencer::state::Get16(encoded.data() + 50176 + 32 + 2) == 1009);
    ProjectCandidate c; c.bank = dense;
    for (unsigned i = 0; i < 228; ++i) c.patch[i] = static_cast<std::uint8_t>(i % 128);
    c.transport = {1, 1, 60, 7, 0x12345678u}; c.routing.bypass = true;
    ProjectBytes bytes, again;
    std::size_t n = 0, n2 = 0;
    auto start = std::chrono::steady_clock::now();
    CHECK(EncodeProject(c, bytes, n, true));
    auto encodedAt = std::chrono::steady_clock::now();
    CHECK(n == 51065); CHECK(GetU16LE(bytes.data() + 4) == 3);
    char goldenPath[2048]; std::snprintf(goldenPath, sizeof(goldenPath), "%s/dense_project.lxrs", argv[1]);
    auto* golden = std::fopen(goldenPath, "rb");
    CHECK(golden != nullptr);
    if (golden)
    {
        ProjectBytes independent {};
        CHECK(std::fread(independent.data(), 1, n, golden) == n);
        CHECK(std::fgetc(golden) == EOF); std::fclose(golden);
        CHECK(std::memcmp(independent.data(), bytes.data(), n) == 0);
    }
    CHECK(GetU32LE(bytes.data() + 8) == 51061);
    CHECK(GetU32LE(bytes.data() + 24) == Crc32V2(bytes.data(), n - 4));
    CHECK(GetU32LE(bytes.data() + n - 4) == 1);
    CHECK(EncodeProject(c, again, n2, true)); CHECK(n == n2 && bytes == again);
    ProjectCandidate decoded;
    auto decodeStart = std::chrono::steady_clock::now();
    CHECK(DecodeProject(bytes.data(), n, decoded, true));
    auto decodedAt = std::chrono::steady_clock::now();
    CHECK(decoded.patch == c.patch); CHECK(equal(decoded.bank, c.bank));
    CHECK(decoded.routing.bypass); CHECK(decoded.transport.enabled == 1);
    CHECK(decoded.transport.clock == 1); CHECK(decoded.transport.bpm == 60);
    CHECK(decoded.transport.pattern == 7); CHECK(decoded.transport.seed == 0x12345678);
    CHECK(bytes[51047] == 60 && bytes[51048] == 0);
    CHECK(GetU32LE(bytes.data() + 51053) == 0x12345678);
    CHECK(EncodeProject(decoded, again, n2, true)); CHECK(bytes == again);
    const auto accepted = decoded;
    for (std::size_t i = 0; i < n; ++i)
        CHECK(!DecodeProject(bytes.data(), i, decoded, true));
    CHECK(decoded.patch == accepted.patch && equal(decoded.bank, accepted.bank));
    CHECK(!DecodeProject(bytes.data(), 65537, decoded, true));
    CHECK(!DecodeProject(nullptr, n, decoded, true));
    // Independent bad fields with repaired outer CRC, so validation is exercised.
    const std::size_t offsets[] = {4,6,8,12,16,18,20,28,32,36,38,40,44,
        564,568,570,572,576,580,588,600,51029,51033,51035,51037,51045,51050};
    for (auto offset : offsets)
    {
        again = bytes; again[offset] ^= 0xff;
        PutU32LE(again.data() + 24, Crc32V2(again.data(), n - 4));
        CHECK(!DecodeProject(again.data(), n, decoded, true));
    }
    again = bytes; again[100] ^= 1; CHECK(!DecodeProject(again.data(), n, decoded, true));
    // Unknown optional/required outer modules reject under the initial v3 policy.
    for (unsigned required = 0; required < 2; ++required)
    {
        again = bytes;
        PutU32LE(again.data() + n - 4, 4); PutU16LE(again.data() + n, 1);
        PutU16LE(again.data() + n + 2, static_cast<std::uint16_t>(required));
        PutU16LE(again.data() + 16, 4);
        PutU32LE(again.data() + 8, static_cast<std::uint32_t>(n + 12));
        PutU32LE(again.data() + 12, static_cast<std::uint32_t>(n - 20));
        PutU32LE(again.data() + 24, Crc32V2(again.data(), n + 12));
        CHECK(!DecodeProject(again.data(), n + 16, decoded, true));
    }
    for (auto offset : {51045u, 51046u, 51049u, 51050u, 51051u, 51057u})
    {
        again = bytes; again[offset] = 255;
        PutU32LE(again.data() + 24, Crc32V2(again.data(), n - 4));
        CHECK(!DecodeProject(again.data(), n, decoded, true));
    }
    again = bytes; PutU16LE(again.data() + 51047, 0);
    PutU32LE(again.data() + 24, Crc32V2(again.data(), n - 4));
    CHECK(!DecodeProject(again.data(), n, decoded, true));
    CHECK(EncodeProject(decoded, again, n2, true)); CHECK(again == bytes);
    // Frozen-v2 opaque optional records survive inside their original module.
    auto extended = c;
    PutU32LE(extended.routing.optionalTlvs.data(), 77);
    PutU16LE(extended.routing.optionalTlvs.data() + 4, 9);
    PutU32LE(extended.routing.optionalTlvs.data() + 8, 3);
    extended.routing.optionalTlvs[16] = 11; extended.routing.optionalTlvs[17] = 22;
    extended.routing.optionalTlvs[18] = 33; extended.routing.optionalTlvBytes = 19;
    CHECK(EncodeProject(extended, again, n2, true)); CHECK(n2 == 51084);
    ProjectCandidate withOptional;
    CHECK(DecodeProject(again.data(), n2, withOptional, true));
    CHECK(withOptional.routing.optionalTlvBytes == 19 &&
          withOptional.routing.optionalTlvs == extended.routing.optionalTlvs);
    // New-format empty bank and default transport remain valid, with no demo.
    ProjectCandidate empty;
    ProjectBytes emptyBytes; std::size_t emptySize = 0;
    CHECK(EncodeProject(empty, emptyBytes, emptySize, true));
    CHECK(DecodeProject(emptyBytes.data(), emptySize, withOptional, true));
    CHECK(equal(withOptional.bank, defaults) && withOptional.transport.enabled == 0);
    // Every bank truncation rejects without publishing a partial object.
    auto bank = defaults;
    for (std::size_t i = 0; i < denseBytes.size(); ++i)
        CHECK(!sequencer::state::Decode(denseBytes.data(), i, bank));
    CHECK(equal(bank, defaults));
    for (auto offset : {33u, 34u, 36u, 50176u+32u+112u, 50176u+32u+112u+56u, 50448u})
    {
        auto bad = denseBytes; bad[offset] = 255;
        sequencer::state::Put32(bad.data() + 20,
            sequencer::state::Crc(bad.data(), bad.size(), 20));
        CHECK(!sequencer::state::Decode(bad.data(), bad.size(), bank));
        CHECK(equal(bank, defaults));
    }
    CHECK(sequencer::state::Decode(musicalBytes.data(), musicalBytes.size(), bank));
    CHECK(bank.patterns[0].mainGates[0] == 0x1111);
    CHECK(bank.patterns[0].steps[0][3].volume == 228);
    CHECK(bank.patterns[0].steps[0][32].parameter1 == 9);
    CHECK(bank.patterns[0].steps[0][32].parameter2 == 38);
    // Historical readers retain their exact formats and supply defaults.
    std::array<std::uint8_t, kEncodedSize> v1;
    CHECK(Encode(c.patch, v1));
    CHECK(DecodeProject(v1.data(), v1.size(), decoded, false));
    CHECK(decoded.patch == c.patch && equal(decoded.bank, defaults));
    CHECK(decoded.transport.enabled == 0 && decoded.transport.bpm == 120);
    CHECK(ValidatePersistentRoutes(decoded.routing.routes) == PersistentRouteStatus::Ok);
    for (const auto& route : decoded.routing.routes) CHECK(route.flags == 0);
    std::array<std::uint8_t, kMaximumStateSize> v2;
    CHECK(EncodeV2(c.patch, c.routing, v2, n2, true));
    CHECK(n2 == 520); CHECK(DecodeProject(v2.data(), n2, decoded, true));
    CHECK(equal(decoded.bank, defaults) && decoded.routing.bypass);
    std::array<double, 228> legacy;
    for (unsigned i = 0; i < 228; ++i) legacy[i] = c.patch[i];
    CHECK(DecodeProject(reinterpret_cast<const std::uint8_t*>(legacy.data()), sizeof(legacy), decoded, false));
    CHECK(decoded.patch == c.patch && equal(decoded.bank, defaults));
    std::printf("{\"pass\":%u,\"fail\":%u,\"core_bytes\":51061,\"stream_bytes\":51065,"
        "\"bank_bytes\":50449,\"encode_us\":%lld,\"decode_us\":%lld,\"candidate_bytes\":%zu,\"encode_buffer_bytes\":%zu}\n", passed, failed,
        static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(encodedAt-start).count()),
        static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(decodedAt-decodeStart).count()),
        sizeof(ProjectCandidate), sizeof(ProjectBytes));
    return failed ? 1 : 0;
}
