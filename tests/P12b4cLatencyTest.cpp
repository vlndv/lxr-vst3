// Independent timing/convolution references for P12b-4c. No plugin test hooks.
#include "../dsp/FIRLatency.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../dsp/ParameterArray.h"
#ifdef LXR_LATENCY_COMPARE_BASELINE
// Optional local audit: snapshot taken before this task's production edits.
#define PolyphaseResampler BaselinePolyphaseResampler
#include "../out/P12b4c-latency-original-resampler.h"
#undef PolyphaseResampler
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
bool rendering = false;
int allocations = 0, passed = 0, failed = 0;
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double sourceRate = static_cast<double>(lxr::kEngineSampleRate);
// Independent specification: 48 taps, equally spaced, endpoints 0 and 47.
constexpr double referenceDelay = (47.0 - 0.0) / 2.0;
constexpr std::array<double, 6> rates {{32000, 44100, 48000, 88200, 96000, 192000}};
void check(const char* name, bool ok) {
  if (!ok) std::printf("FAIL %s\n", name);
  ok ? ++passed : ++failed;
}
double pcm(std::uint64_t index) {
  // Deterministic signed 16-bit PCM; no PRNG global state or allocation.
  return (static_cast<int>((index * 173u + 7919u) % 65536u) - 32768) / 32768.0;
}
// Direct finite convolution from the published coefficient design and absolute
// time, with no circular history, streaming position, or production helpers.
double reference(double rate, int frame, int impulse) {
  const double time = frame * sourceRate / rate;
  const int base = static_cast<int>(std::floor(time));
  const double frac = std::floor((time - base) * 512.0) / 512.0;
  const double cutoff = .47 * std::min(1.0, rate / sourceRate);
  double sum = 0, result = 0;
  for (int k = 0; k < 48; ++k) {
    const double angle = 2 * pi * cutoff * (k + frac - referenceDelay);
    const double sinc = std::abs(angle) < 1e-12 ? 1 : std::sin(angle) / angle;
    const double window = .42 - .5 * std::cos(2*pi*k/47) + .08 * std::cos(4*pi*k/47);
    const double c = 2 * cutoff * sinc * window;
    sum += c;
    if (base >= k)
      result += c * (impulse < 0 ? pcm(base-k) : (base-k == impulse ? 1.0 : 0.0));
  }
  return result / sum;
}
void runRate(double rate, lxr::PolyphaseResampler& fir) {
  const double exact = referenceDelay / sourceRate * rate;
  // Independently select the closer of the two neighboring integer frames.
  const int below = static_cast<int>(exact);
  const int expected = (exact-below < (below+1)-exact) ? below : below+1;
  check("exact nominal host frames", std::abs(lxr::firLatency::exactHostFrames(rate)-exact) < 1e-12);
  check("nearest integer host samples", lxr::firLatency::hostSamples(rate) == expected);
  check("fractional residual <= half frame", std::abs(exact-expected) <= .5);
  check("nominal milliseconds", std::abs(lxr::firLatency::nominalMilliseconds(rate)-1000*referenceDelay/sourceRate) < 1e-12);
  int startupPeak = 0;
  double startupMoment = 0;
  for (int impulse : {0, 31, 32, 137}) {
    check("rate configure", fir.configure(rate));
    std::uint64_t consumed = 0;
    auto source = [&](double& l, double& r) noexcept {
      l = consumed++ == static_cast<std::uint64_t>(impulse) ? 1 : 0; r = 0;
    };
    double maxMagnitude = 0, refMax = 0, maxError = 0, sum = 0, moment = 0;
    int peak = 0, refPeak = 0;
    bool isolated = true;
    rendering = true;
    for (int n = 0; n < 1024; ++n) {
      double l, r; fir.next(source, l, r);
      const double ref = reference(rate, n, impulse);
      maxError = std::max(maxError, std::abs(l-ref));
      isolated &= std::isfinite(l) && r == 0;
      if (std::abs(l) > maxMagnitude) { maxMagnitude = std::abs(l); peak = n; }
      if (std::abs(ref) > refMax) { refMax = std::abs(ref); refPeak = n; }
      sum += l; moment += n*l;
    }
    rendering = false;
    check("impulse direct convolution", maxError < 1e-10);
    check("impulse independent peak", peak == refPeak && maxMagnitude > .01);
    check("impulse stereo isolation", isolated);
    check("impulse timing without extra buffering", std::abs(peak-(impulse+referenceDelay)*rate/sourceRate) < .75);
    if (impulse == 0) { startupPeak = peak; startupMoment = moment/sum; }
  }
  std::printf("LATENCY rate=%.0f nominal=%.12f reported=%d residual=%+.12f peak=%d signed_moment=%.12f ms=%.12f\n",
    rate, exact, expected, exact-expected, startupPeak, startupMoment,
    1000*referenceDelay/sourceRate);

  fir.configure(rate);
  std::uint64_t index = 0;
  auto source = [&](double& l, double& r) noexcept { l = pcm(index++); r = -l; };
  std::array<double, 1024> first {};
  bool stable = true;
  rendering = true;
  for (int n = 0; n < 1024; ++n) {
    double l, r; fir.next(source, l, r); first[n] = l;
    stable &= std::abs(l-reference(rate, n, -1)) < 1e-10 && r == -l;
  }
  fir.reset(); index = 0;
  for (int n = 0; n < 1024; ++n) {
    double l, r; fir.next(source, l, r); stable &= l == first[n] && r == -l;
  }
  rendering = false;
  check("PCM independent reference and exact reset replay", stable);
  check("reset retains rate", fir.ready() && fir.hostRate() == rate);
#ifdef LXR_LATENCY_COMPARE_BASELINE
  lxr::BaselinePolyphaseResampler old;
  old.configure(rate); index = 0;
  bool unchanged = true;
  for (int n = 0; n < 1024; ++n) {
    double l, r; old.next(source, l, r); unchanged &= l == first[n] && r == -l;
  }
  check("PCM bit-identical to pre-task FIR", unchanged);
#endif
}
} // namespace

void* operator new(std::size_t n) {
  if (rendering) ++allocations;
  if (void* p = std::malloc(n ? n : 1)) return p;
  std::abort();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
  check("48-tap specification", lxr::PolyphaseResampler::kTaps == 48);
  check("23.5 source-frame nominal delay", lxr::PolyphaseResampler::groupDelaySourceFrames() == referenceDelay);
  check("native rate single source", lxr::PolyphaseResampler::kSourceRate == sourceRate);
  lxr::PolyphaseResampler fir;
  check("initially not ready", !fir.ready());
  // At the native rate, phase remains zero and the real impulse response is
  // symmetric. Its signed first moment independently measures 23.5 samples.
  fir.configure(sourceRate);
  std::array<double, 48> nativeImpulse {};
  int nativeIndex = 0;
  auto nativeSource = [&](double& l, double& r) noexcept { l = nativeIndex++ == 0 ? 1 : 0; r = 0; };
  double sum = 0, moment = 0;
  for (int n = 0; n < 48; ++n) {
    double r; fir.next(nativeSource, nativeImpulse[n], r);
    sum += nativeImpulse[n]; moment += n*nativeImpulse[n];
  }
  bool symmetric = true;
  for (int n = 0; n < 48; ++n)
    symmetric &= std::abs(nativeImpulse[n]-nativeImpulse[47-n]) < 1e-14;
  check("native impulse symmetry", symmetric);
  check("measured native impulse group delay", std::abs(moment/sum-referenceDelay) < 1e-12);
  std::printf("NATIVE measured_signed_moment=%.12f source_frames\n", moment/sum);
  for (double rate : rates) runRate(rate, fir); // same instance changes rates
  const double invalid[] = {0, -1, 1, 31999.99, 192000.01, 1e300,
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
  for (double rate : invalid) {
    fir.configure(48000);
    check("invalid helper rate", !lxr::firLatency::supported(rate) &&
      lxr::firLatency::exactHostFrames(rate) == 0 && lxr::firLatency::hostSamples(rate) == 0 &&
      lxr::firLatency::nominalMilliseconds(rate) == 0);
    check("invalid transition clears readiness", !fir.configure(rate) && !fir.ready() && fir.hostRate() == 0);
    fir.reset();
    int calls = 0;
    auto source = [&](double&, double&) noexcept { ++calls; };
    double l = 1, r = 1; fir.next(source, l, r);
    check("invalid reset produces silence without consuming", !fir.ready() && l == 0 && r == 0 && calls == 0);
  }
  // Native adapter cache is demand-filled: the first host output consumes the
  // first native sample immediately, rather than waiting an extra 32 frames.
  lxr::Engine engine;
  lxr::ParameterArray params;
  if (!engine.loadAssets("data")) { check("assets loaded", false); return 1; }
  lxr::LXRAudioAdapter adapter;
  for (double rate : rates) {
    engine.init(); params.init(); params.applyAllToEngine(engine);
    engine.triggerDrum(0, 110, 60);
    std::array<int16_t, 32> l {}, r {}, c {}, d {};
    engine.processBlock(l.data(), r.data(), c.data(), d.data());
    engine.init(); params.applyAllToEngine(engine); engine.triggerDrum(0, 110, 60);
    adapter.configure(rate); adapter.reset();
    bool same = true;
    rendering = true;
    for (int n = 0; n < 32; ++n) {
      int16_t a, b; adapter.getNextST1Sample(engine, a, b); same &= a == l[n] && b == r[n];
    }
    rendering = false;
    check("adapter preserves native PCM and first-sample timing", same);
    engine.init(); params.applyAllToEngine(engine);
    adapter.reset();
    bool silent = true;
    rendering = true;
    for (int n = 0; n < 1024; ++n) {
      double a, b; adapter.getNextHostST1Sample(engine, a, b); silent &= a == 0 && b == 0;
    }
    rendering = false;
    check("reset clears cached audio and FIR tail", silent && adapter.ready() && adapter.hostRate() == rate);
    check("adapter rejects stale configuration", !adapter.configure(0) && !adapter.ready());
    check("adapter recovers supported configuration", adapter.configure(rate) && adapter.hostRate() == rate);
  }
  check("no C++ allocation during measured render/reset paths", allocations == 0);
  std::printf("SUMMARY pass=%d fail=%d renderAllocations=%d\n", passed, failed, allocations);
  return failed ? 1 : 0;
}
