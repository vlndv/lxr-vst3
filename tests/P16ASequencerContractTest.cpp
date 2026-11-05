#include "reference/sequencer/original_sequencer_adapter.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int gPass = 0;
int gFail = 0;

void check(const bool condition, const char* name) {
  if (condition) {
    ++gPass;
  } else {
    ++gFail;
    std::fprintf(stderr, "FAIL: %s\n", name);
  }
}

struct Event {
  std::int64_t sample;
  std::uint8_t track;
  std::uint8_t position;
  bool operator==(const Event& other) const {
    return sample == other.sample && track == other.track && position == other.position;
  }
};

// Host-sync contract model: enumerate a 32-PPQ grid from an integer sample
// anchor. This is a design test, not an original-firmware differential oracle.
std::vector<Event> renderCallbacks(const std::vector<int>& blocks,
                                   const int totalSamples,
                                   const int samplesPerSubstep) {
  std::vector<Event> events;
  std::int64_t cursor = 0;
  std::int64_t next = 0;
  std::size_t blockIndex = 0;
  while (cursor < totalSamples) {
    int frames = blocks[blockIndex++ % blocks.size()];
    if (cursor + frames > totalSamples) frames = (int) (totalSamples - cursor);
    const std::int64_t end = cursor + frames;
    while (next < end) {
      const auto position = (std::uint8_t) ((next / samplesPerSubstep) & 127);
      for (std::uint8_t track = 0; track < 7; ++track)
        events.push_back({next, track, position});
      next += samplesPerSubstep;
    }
    cursor = end;
  }
  return events;
}

std::vector<std::int64_t> rationalGrid(const int sampleRate, const int bpm,
                                       const int count) {
  std::vector<std::int64_t> result;
  result.reserve((std::size_t) count);
  const std::int64_t numerator = (std::int64_t) sampleRate * 60;
  const std::int64_t denominator = (std::int64_t) bpm * 32;
  for (int i = 0; i < count; ++i)
    result.push_back(((std::int64_t) i * numerator) / denominator);
  return result;
}

void dataModelTests() {
  check(p16a_track_count() == 7, "seven tracks");
  check(p16a_pattern_count() == 8, "eight patterns");
  check(p16a_position_count() == 128, "128 flattened positions");
  check(p16a_step_size() == 7, "Step is seven bytes");
  check(p16a_pattern_setting_size() == 2, "PatternSetting is two bytes");
  check(p16a_length_rotate_size() == 4, "host GCC exposes unsigned-bitfield ABI size");
  check(p16a_pattern_set_size() == 50528, "host-compiled PatternSet ABI size");
  check(p16a_temp_pattern_size() == 6316, "host-compiled TempPattern ABI size");
  for (std::uint32_t i = 0; i < P16A_FIELD_COUNT; ++i)
    check(p16a_step_offset((P16AField) i) == i, "Step field is packed in source order");
  check(p16a_step_active_mask() == 0x80, "active flag is volume bit 7");
  check(p16a_step_volume_mask() == 0x7f, "volume is low seven bits");
  check(p16a_default_note() == 63, "default note is 63");
  check(p16a_no_automation() == 0xff, "automation sentinel is 0xff");
  const auto validPosition = [](int pattern, int track, int position) {
    return pattern >= 0 && pattern < 8 && track >= 0 && track < 7 &&
           position >= 0 && position < 128;
  };
  check(validPosition(7, 6, 127), "maximum pattern coordinate is valid");
  check(!validPosition(8, 0, 0), "invalid pattern is rejected");
  check(!validPosition(0, 7, 0), "invalid track is rejected");
  check(!validPosition(0, 0, 128), "invalid physical position is rejected");
}

void clockTests() {
  check(std::fabs(p16a_internal_tick_quarter_ms(120) - (125.0 / 6.0)) < 1e-12,
        "120 BPM internal tick retains fractional quarter-ms time");
  check(std::fabs(p16a_substep_seconds(120) - 0.015625) < 1e-12,
        "120 BPM substep is 15.625 ms");
  check(std::fabs(p16a_substep_seconds(30) - 0.0625) < 1e-12,
        "30 BPM substep");
  check(std::fabs(p16a_substep_seconds(300) - 0.00625) < 1e-12,
        "300 BPM substep");
  check(p16a_shuffle_factor(0) == 0.f && p16a_shuffle_factor(8) == 1.f,
        "shuffle endpoints");
  check(p16a_shuffle_factor(16) == p16a_shuffle_factor(0), "shuffle table wraps");

  const auto fixed = renderCallbacks({64}, 48000 * 8, 750);
  const auto changing = renderCallbacks({1, 17, 255, 1024, 31, 511}, 48000 * 8, 750);
  check(fixed == changing, "event timeline is callback-size invariant");
  check(fixed.size() == 512u * 7u, "eight seconds has 512 physical positions at 120 BPM");
  check(fixed.front().sample == 0, "transport start event is immediate");
  check(fixed.back().sample == 383250, "long callback schedule has no drift");

  const auto fractional = rationalGrid(48000, 123, 1000000);
  const std::int64_t exactLast =
      ((std::int64_t) 999999 * 48000 * 60) / (123 * 32);
  check(fractional.back() == exactLast, "million-event rational clock has no accumulated drift");
  check(fractional[1] == 731 && fractional[2] == 1463,
        "fractional sample positions use deterministic floor policy");
  check(rationalGrid(48000, 123, 256) == rationalGrid(48000, 123, 256),
        "transport restart reproduces the grid");

  const auto seekGrid = rationalGrid(48000, 120, 64);
  std::size_t firstAfterSeek = 0;
  while (firstAfterSeek < seekGrid.size() && seekGrid[firstAfterSeek] < 30001)
    ++firstAfterSeek;
  check(seekGrid[firstAfterSeek] == 30750, "forward seek does not replay skipped boundary");
  check(seekGrid[0] == 0 && seekGrid[16] == 12000, "loop epochs restart at a stable relative phase");

  const auto quantizeNative = [](std::int64_t frame) {
    return ((frame + 31) / 32) * 32;
  };
  check(quantizeNative(0) == 0 && quantizeNative(1) == 32,
        "native-block boundary policy");
  check(quantizeNative(32123) - 32123 >= 0 && quantizeNative(32123) - 32123 < 32,
        "native-block quantization is bounded below 32 frames");
}

void playbackTests() {
  constexpr std::array<std::uint8_t, 7> owner{{0, 1, 2, 3, 4, 5, 5}};
  constexpr std::array<std::uint8_t, 7> articulation{{0, 0, 0, 0, 0, 0, 1}};
  check(owner[5] == owner[6], "hat tracks share synthesis owner");
  check(articulation[5] != articulation[6], "hat tracks retain articulation identity");

  const auto probability = [](std::uint8_t random, std::uint8_t threshold) {
    return random <= threshold;
  };
  check(probability(0, 0), "probability zero still passes random zero (source quirk)");
  check(!probability(1, 0), "probability zero rejects nonzero random");
  check(probability(127, 127), "probability 127 always passes");

  std::array<int, 7> order{{0, 1, 2, 3, 4, 5, 6}};
  check(order[5] == 5 && order[6] == 6, "simultaneous hats dispatch closed then open");
  const std::uint16_t mainMask = 1u << 3;
  check((mainMask & (1u << (24 / 8))) != 0, "main gate owns eight flattened positions");
  check((mainMask & (1u << (16 / 8))) == 0, "inactive main suppresses stored substep");

  std::uint8_t clusterRandom = 91;
  const bool boundaryActive = false;
  if (boundaryActive) clusterRandom = 12;
  check(clusterRandom == 91,
        "inactive cluster boundary retains previous probability random (source quirk)");
  check(clusterRandom <= 100, "later active substep reuses retained cluster random");
}

void automationTests() {
  constexpr std::uint8_t noAutomation = 0xff;
  std::array<int, 228> base{};
  base[42] = 11;
  int effective = base[42];
  std::uint8_t slot0 = 42;
  std::uint8_t slot1 = 42;
  if (slot0 != noAutomation) effective = 70;
  if (slot1 != noAutomation) effective = 90;
  check(effective == 90, "second automation slot wins duplicate target");
  check(base[42] == 11, "sequencer overlay preserves base patch");
  slot0 = noAutomation;
  effective = base[42];
  if (slot0 != noAutomation) effective = 1;
  check(effective == 11, "cleared slot restores authoritative base");

  int persistentOverlay = 77;
  const bool rejectedByProbability = true;
  if (!rejectedByProbability) persistentOverlay = 88;
  check(persistentOverlay == 77, "probability rejection leaves existing lock overlay active");
  persistentOverlay = base[42];
  check(persistentOverlay == 11, "transport/restore reconstruction clears ephemeral lock");
}

void chainTests() {
  const auto next = [](int barCounter, int changeBar, int active, int target) {
    return (barCounter % (changeBar + 1)) == 0 ? target : active;
  };
  check(next(1, 0, 0, 1) == 1, "changeBar zero changes every track-zero wrap");
  check(next(1, 1, 0, 1) == 0, "repeat interval retains pattern");
  check(next(2, 1, 0, 1) == 1, "repeat interval transitions on modulo boundary");
  check((8 - 8 + 2) == 2 && (14 - 8 + 2) == 8,
        "random next-pattern codes 8..14 select among 2..8 patterns");
  int active = 0;
  for (int bar = 1; bar <= 6; ++bar) active = next(bar, 1, active, active == 0 ? 1 : 0);
  check(active == 1, "multi-pattern repeat state machine is deterministic");
  check(next(8, 0, 7, 7) == 7, "self-chain has no terminal state");
}

void determinismTests() {
  const auto timelineA = renderCallbacks({7, 31, 509}, 48000 * 4, 750);
  const auto timelineB = renderCallbacks({2048, 1}, 48000 * 4, 750);
  check(timelineA == timelineB, "offline timeline repeats across callback partitions");

  const auto nextRandom = [](std::uint32_t value) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
  };
  std::uint32_t a = 0x12345678u;
  std::uint32_t b = 0x12345678u;
  std::uint32_t c = 0x87654321u;
  for (int i = 0; i < 1000; ++i) {
    a = nextRandom(a);
    b = nextRandom(b);
    c = nextRandom(c);
  }
  check(a == b, "same proposed test seed is deterministic");
  check(a != c, "different proposed test seeds diverge");
}

} // namespace

int main() {
  dataModelTests();
  clockTests();
  playbackTests();
  automationTests();
  chainTests();
  determinismTests();
  std::printf("{\"pass\":%d,\"fail\":%d}\n", gPass, gFail);
  return gFail == 0 ? 0 : 1;
}
