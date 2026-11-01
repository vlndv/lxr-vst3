#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int passed = 0;
int failed = 0;

void check(const char* name, bool condition)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    condition ? ++passed : ++failed;
}

using Parameters = std::array<uint8_t, lxr::state::kParameterCount>;
using Encoded = std::array<uint8_t, lxr::state::kEncodedSize>;

Parameters defaults()
{
    lxr::ParameterArray params;
    params.init();
    Parameters result {};
    std::copy(std::begin(params.values), std::end(params.values), result.begin());
    result[0] = 0;
    return result;
}

bool roundTrip(const Parameters& source)
{
    Encoded encoded {};
    Parameters decoded {};
    if (!lxr::state::Encode(source, encoded))
        return false;
    const auto result = lxr::state::Decode(encoded.data(), encoded.size(), decoded);
    return result && result.consumed == encoded.size() && !result.legacy && decoded == source;
}

void refreshCrc(Encoded& encoded)
{
    lxr::state::PutU32LE(
        encoded.data() + 20,
        lxr::state::Crc32(encoded.data() + lxr::state::kHeaderSize,
                          lxr::state::kPayloadSize));
}

struct Render
{
    std::array<int16_t, 4096> left {};
    std::array<int16_t, 4096> right {};
    int peak = 0;
    double rms = 0.0;
};

Render renderVoice(const Parameters& values, int voice)
{
    lxr::Engine engine;
    lxr::ParameterArray params;
    const bool loaded = engine.loadAssets("data");
    if (!loaded)
        return {};

    params.init();
    for (std::size_t i = 0; i < values.size(); ++i)
        params.values[i] = values[i];
    params.applyAllToEngine(engine);

    switch (voice)
    {
        case 0: engine.triggerDrum(0, 109, 60); break;
        case 1: engine.triggerDrum(1, 101, 64); break;
        case 2: engine.triggerDrum(2, 93, 67); break;
        case 3: engine.triggerSnare(117, 60); break;
        case 4: engine.triggerCymbal(105, 62); break;
        case 5: engine.triggerHiHat(111, false, 60); break;
        default: engine.triggerHiHat(111, true, 60); break;
    }

    Render render;
    double energy = 0.0;
    for (int block = 0; block < 128; ++block)
    {
        int16_t st1l[32] {};
        int16_t st1r[32] {};
        int16_t st2l[32] {};
        int16_t st2r[32] {};
        engine.processBlock(st1l, st1r, st2l, st2r);
        for (int i = 0; i < 32; ++i)
        {
            const int frame = block * 32 + i;
            render.left[frame] = st1l[i];
            render.right[frame] = st1r[i];
            render.peak = std::max({render.peak, std::abs(static_cast<int>(st1l[i])),
                                    std::abs(static_cast<int>(st1r[i]))});
            energy += static_cast<double>(st1l[i]) * st1l[i];
            energy += static_cast<double>(st1r[i]) * st1r[i];
        }
    }
    render.rms = std::sqrt(energy / (2.0 * render.left.size()));
    return render;
}

bool equalRender(const Render& a, const Render& b)
{
    return a.left == b.left && a.right == b.right && a.peak == b.peak && a.rms == b.rms;
}

} // namespace

int main()
{
    Parameters patch = defaults();
    check("format parameter count is 228 including reserved slot", patch.size() == 228);
    check("active parameter count is 227", lxr::state::kActiveParameterCount == 227);
    check("reserved parameter default is zero", patch[0] == 0);
    check("default patch round trip", roundTrip(patch));

    Parameters reserved = patch;
    reserved[0] = 127;
    check("reserved slot 0 is preserved consistently", roundTrip(reserved));

    Parameters minimum {};
    check("all active parameters at minimum", roundTrip(minimum));

    Parameters maximum {};
    std::fill(maximum.begin() + 1, maximum.end(), 127);
    check("all active parameters at maximum", roundTrip(maximum));

    Parameters mixed {};
    for (std::size_t i = 1; i < mixed.size(); ++i)
        mixed[i] = static_cast<uint8_t>((i * 73u + 19u) & 0x7fu);
    check("mixed boundary patch round trip", roundTrip(mixed));

    uint32_t random = 0x4c585231u;
    Parameters randomPatch {};
    for (std::size_t i = 1; i < randomPatch.size(); ++i)
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        randomPatch[i] = static_cast<uint8_t>(random & 0x7fu);
    }
    check("deterministic random patch round trip", roundTrip(randomPatch));

    bool everyValue = true;
    Parameters single = defaults();
    for (std::size_t id = 1; id < single.size() && everyValue; ++id)
    {
        const uint8_t saved = single[id];
        for (int value = 0; value <= 127; ++value)
        {
            single[id] = static_cast<uint8_t>(value);
            if (!roundTrip(single))
            {
                everyValue = false;
                break;
            }
        }
        single[id] = saved;
    }
    check("every 7-bit value survives for every active parameter ID", everyValue);

    Encoded first {};
    Encoded second {};
    check("encode deterministic input one", lxr::state::Encode(randomPatch, first));
    check("encode deterministic input two", lxr::state::Encode(randomPatch, second));
    check("identical state produces identical bytes", first == second);

    Parameters cycle = randomPatch;
    bool repeatedCycles = true;
    for (int i = 0; i < 100; ++i)
    {
        Encoded bytes {};
        Parameters restored {};
        repeatedCycles &= lxr::state::Encode(cycle, bytes);
        const auto result = lxr::state::Decode(bytes.data(), bytes.size(), restored);
        repeatedCycles &= static_cast<bool>(result) && restored == cycle;
        cycle = restored;
    }
    check("one hundred repeated save/load cycles", repeatedCycles && cycle == randomPatch);

    Parameters sentinel {};
    sentinel.fill(0x55);
    const Parameters originalSentinel = sentinel;
    check("empty payload rejected",
          !lxr::state::Decode(nullptr, 0, sentinel) && sentinel == originalSentinel);

    bool everyTruncationRejected = true;
    for (std::size_t size = 1; size < first.size(); ++size)
    {
        sentinel = originalSentinel;
        everyTruncationRejected &= !lxr::state::Decode(first.data(), size, sentinel);
        everyTruncationRejected &= sentinel == originalSentinel;
    }
    check("every truncated versioned payload rejected atomically", everyTruncationRejected);

    std::vector<uint8_t> oversized(lxr::state::kMaximumStateSize + 1, 0);
    check("oversized payload rejected",
          !lxr::state::Decode(oversized.data(), oversized.size(), sentinel));

    Encoded corrupt = first;
    corrupt[0] = 'X';
    check("invalid magic rejected", !lxr::state::Decode(corrupt.data(), corrupt.size(), sentinel));
    corrupt = first;
    lxr::state::PutU16LE(corrupt.data() + 4, lxr::state::kFormatVersion + 1);
    check("unsupported future version rejected", !lxr::state::Decode(corrupt.data(), corrupt.size(), sentinel));
    corrupt = first;
    lxr::state::PutU32LE(corrupt.data() + 8, 227);
    check("corrupt payload length rejected", !lxr::state::Decode(corrupt.data(), corrupt.size(), sentinel));
    corrupt = first;
    corrupt[40] ^= 0x40;
    check("checksum corruption rejected", !lxr::state::Decode(corrupt.data(), corrupt.size(), sentinel));
    corrupt = first;
    corrupt[lxr::state::kHeaderSize + 17] = 255;
    refreshCrc(corrupt);
    check("invalid parameter byte rejected", !lxr::state::Decode(corrupt.data(), corrupt.size(), sentinel));
    std::array<uint8_t, lxr::state::kEncodedSize + 1> trailing {};
    std::copy(first.begin(), first.end(), trailing.begin());
    check("unexpected trailing byte rejected",
          !lxr::state::Decode(trailing.data(), trailing.size(), sentinel));

    std::array<uint8_t, lxr::state::kEncodedSize + lxr::state::kVST3BypassSize> vst3State {};
    std::copy(first.begin(), first.end(), vst3State.begin());
    auto vst3Result = lxr::state::Decode(vst3State.data(), vst3State.size(), sentinel, true);
    check("documented VST3 bypass suffix accepted", vst3Result && vst3Result.consumed == first.size());

    std::array<uint8_t, lxr::state::kLegacyEncodedSize> legacy {};
    for (std::size_t i = 0; i < randomPatch.size(); ++i)
    {
        const double value = randomPatch[i];
        std::memcpy(legacy.data() + i * sizeof(double), &value, sizeof(double));
    }
    Parameters legacyDecoded {};
    const auto legacyResult = lxr::state::Decode(legacy.data(), legacy.size(), legacyDecoded);
    check("legacy iPlug2 double stream migrates", legacyResult && legacyResult.legacy &&
          legacyDecoded == randomPatch);
    double invalidLegacy = 127.5;
    std::memcpy(legacy.data() + 42 * sizeof(double), &invalidLegacy, sizeof(double));
    check("invalid legacy parameter rejected",
          !lxr::state::Decode(legacy.data(), legacy.size(), legacyDecoded));

    // Exercise all major Engine parameter families with a deterministic patch.
    Parameters audioPatch = defaults();
    const std::array<std::pair<int, int>, 39> edits {{
        {1, 0}, {2, 2}, {3, 3}, {4, 1}, {6, 0}, {7, 0},
        {8, 58}, {10, 65}, {12, 71}, {14, 69}, {16, 76}, {18, 82},
        {27, 91}, {28, 48}, {37, 52}, {40, 77}, {43, 35}, {46, 82},
        {49, 0}, {50, 55}, {55, 0}, {56, 44}, {68, 2}, {73, 39},
        {74, 27}, {77, 31}, {82, 22}, {88, 107}, {91, 116},
        {102, 34}, {105, 63}, {108, 127}, {111, 127}, {114, 127},
        {115, 28}, {121, 0}, {137, 1}, {191, 7}, {194, 1}
    }};
    for (const auto& edit : edits)
        audioPatch[static_cast<std::size_t>(edit.first)] = static_cast<uint8_t>(edit.second);
    audioPatch[197] = 45; audioPatch[200] = 52;
    audioPatch[203] = 3; audioPatch[206] = 11;
    audioPatch[209] = 61; audioPatch[212] = 70;
    audioPatch[215] = 0; audioPatch[218] = 0;
    audioPatch[221] = 60; audioPatch[224] = 60;

    Encoded audioBytes {};
    Parameters restoredAudioPatch {};
    const bool audioStateOk = lxr::state::Encode(audioPatch, audioBytes) &&
        lxr::state::Decode(audioBytes.data(), audioBytes.size(), restoredAudioPatch);
    check("controlled audio patch serialized", audioStateOk && restoredAudioPatch == audioPatch);

    bool allVoicesExact = audioStateOk;
    int aggregatePeak = 0;
    double aggregateRms = 0.0;
    for (int voice = 0; voice < 7; ++voice)
    {
        const Render before = renderVoice(audioPatch, voice);
        const Render after = renderVoice(restoredAudioPatch, voice);
        allVoicesExact &= equalRender(before, after);
        aggregatePeak = std::max(aggregatePeak, before.peak);
        aggregateRms += before.rms;
    }
    std::printf("AUDIO_EQ aggregate_peak=%d aggregate_rms=%.12f\n",
                aggregatePeak, aggregateRms);
    check("D1-D3 snare cymbal closed/open hat PCM exact after restore", allVoicesExact);
    check("restored controlled patch produces nonzero audio", aggregatePeak > 0 && aggregateRms > 0.0);

    std::printf("STATE format_version=%u encoded_bytes=%zu active_params=%zu\n",
                static_cast<unsigned>(lxr::state::kFormatVersion),
                lxr::state::kEncodedSize,
                lxr::state::kActiveParameterCount);
    std::printf("SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
