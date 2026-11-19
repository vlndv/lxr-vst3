#include "reference/presets/original_preset_adapter.h"
#include "reference/morph/original_morph_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static uint64_t assertions = 0;
static void check(bool ok, const char* what) {
  ++assertions;
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s (assertion %llu)\n", what,
                 static_cast<unsigned long long>(assertions));
    std::exit(1);
  }
}

/* Independent, widened expression of the byte contract. The function under
 * test is the unchanged file-local firmware implementation. */
static uint8_t expected(uint8_t a, uint8_t b, uint8_t x) {
  const int32_t fixed = int32_t(a) * 256 +
                        (int32_t(b) - int32_t(a)) * int32_t(x);
  const uint8_t integral = static_cast<uint8_t>(fixed / 256);
  return static_cast<uint8_t>((fixed & 0xff) < 0x7f ? integral
                                                    : integral + 1);
}

static bool isRoute(unsigned id) {
  for (unsigned family = 0; family < 3; ++family)
    for (unsigned voice = 0; voice < 6; ++voice)
      if (id == p18_original_route_id(family, voice)) return true;
  return false;
}

int main() {
  struct Vector { uint8_t a, b, x, result; };
  constexpr Vector vectors[] = {
      {0, 255, 0, 0}, {0, 255, 1, 1}, {0, 255, 127, 127},
      {0, 255, 128, 128}, {0, 255, 255, 254}, {255, 0, 255, 1},
      {0, 1, 126, 0}, {0, 1, 127, 1}, {1, 0, 129, 1},
      {1, 0, 130, 0}, {64, 64, 255, 64}, {10, 20, 255, 20}};
  for (const auto& v : vectors)
    check(p18_original_interpolate(v.a, v.b, v.x) == v.result,
          "explicit firmware interpolation vector");

  for (unsigned a = 0; a < 256; ++a)
    for (unsigned b = 0; b < 256; ++b)
      for (unsigned x = 0; x < 256; ++x)
        check(p18_original_interpolate(static_cast<uint8_t>(a),
                                       static_cast<uint8_t>(b),
                                       static_cast<uint8_t>(x)) ==
                  expected(static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                           static_cast<uint8_t>(x)),
              "exhaustive original interpolation equivalence");

  std::array<uint8_t, 228> source{}, target{};
  for (unsigned i = 0; i < source.size(); ++i) {
    source[i] = static_cast<uint8_t>((i * 17u + 3u) & 0x7f);
    target[i] = static_cast<uint8_t>((i * 43u + 91u) & 0x7f);
  }
  std::array<P17Message, 228> messages{};
  const uint8_t amount = 173;
  check(p18_original_run_morph(source.data(), target.data(), amount,
                               messages.data(), messages.size()) == 228,
        "morph emits every sound parameter");
  for (unsigned i = 0; i < messages.size(); ++i) {
    check(messages[i].status == (i < 128 ? 0xb0 : 0xb6),
          "split MIDI_CC/CC_2 status");
    check(messages[i].first == (i < 128 ? i : i - 128),
          "split parameter index");
    check(messages[i].second == expected(source[i], target[i], amount),
          "morph message value");
  }

  const uint8_t name[8] = {'M','O','R','P','H',0,0xff,0x7f};
  std::array<uint8_t, 1024> file{};
  const size_t written = p18_original_save_morph(
      source.data(), target.data(), amount, name, file.data(), file.size());
  check(written == 236, "morphed sound file is 236 bytes");
  for (unsigned i = 0; i < 8; ++i)
    check(file[i] == name[i], "morphed sound preserves raw name");
  unsigned preserved_routes = 0;
  for (unsigned i = 0; i < source.size(); ++i) {
    const uint8_t want = isRoute(i) ? source[i]
                                    : expected(source[i], target[i], amount);
    if (isRoute(i)) ++preserved_routes;
    check(file[8 + i] == want, "morphed sound byte policy");
  }
  check(preserved_routes == 18, "all 18 selector/hint bytes stay at source");

  unsigned ascending_endpoint_misses = 0;
  unsigned descending_endpoint_misses = 0;
  for (unsigned a = 0; a < 256; ++a) {
    for (unsigned b = 0; b < 256; ++b) {
      const auto got = p18_original_interpolate(
          static_cast<uint8_t>(a), static_cast<uint8_t>(b), 255);
      if (a < b && got != b) ++ascending_endpoint_misses;
      if (a > b && got != b) ++descending_endpoint_misses;
    }
  }
  check(ascending_endpoint_misses == 8001,
        "ascending x255 endpoint miss count");
  check(descending_endpoint_misses == 8385,
        "descending x255 endpoint miss count");

  std::printf(
      "{\"assertions\":%llu,\"exhaustive_inputs\":16777216,"
      "\"morph_messages\":228,\"preserved_route_bytes\":18,"
      "\"ascending_endpoint_misses\":%u,"
      "\"descending_endpoint_misses\":%u}\n",
      static_cast<unsigned long long>(assertions),
      ascending_endpoint_misses, descending_endpoint_misses);
  return 0;
}
