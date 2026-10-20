#include "../dsp/PolyphaseResampler.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

void require(bool condition, const char* message)
{
  if (!condition)
  {
    std::fprintf(stderr, "FAIL %s\n", message);
    std::exit(1);
  }
}

void runRate(double hostRate)
{
  lxr::PolyphaseResampler resampler;

  require(
    resampler.configure(hostRate),
    "valid host rate"
  );

  std::uint64_t consumed = 0;

  auto source =
    [&](double& left, double& right) noexcept
    {
      left = 0.5;
      right = -0.25;
      ++consumed;
    };

  double left = 0.0;
  double right = 0.0;

  for (int i = 0; i < 10000; ++i)
  {
    resampler.next(source, left, right);

    require(
      std::isfinite(left) && std::isfinite(right),
      "finite output"
    );

    // Allow enough source-time for FIR startup.
    if (i > 400)
    {
      require(
        std::abs(left - 0.5) < 0.001 &&
        std::abs(right + 0.25) < 0.001,
        "DC gain and stereo"
      );
    }
  }

  const double step =
    lxr::PolyphaseResampler::kSourceRate /
    hostRate;

  require(
    std::abs(
      resampler.sourcePosition() -
      10000.0 * step
    ) < 1.0e-6,
    "rate drift"
  );

  const std::uint64_t expected =
    static_cast<std::uint64_t>(
      std::floor(9999.0 * step)
    ) + 1;

  require(
    consumed == expected,
    "native source consumption"
  );

  std::printf("PASS rate %.0f\n", hostRate);
}

void runChunking()
{
  lxr::PolyphaseResampler a;
  lxr::PolyphaseResampler b;

  require(
    a.configure(48000.0) &&
    b.configure(48000.0),
    "chunk configuration"
  );

  std::uint64_t na = 0;
  std::uint64_t nb = 0;

  auto makeSource =
    [](std::uint64_t& index)
    {
      return [&index](
        double& left,
        double& right
      ) noexcept
      {
        left = std::sin(
          0.01 * static_cast<double>(index)
        );

        right = -left;
        ++index;
      };
    };

  auto sa = makeSource(na);
  auto sb = makeSource(nb);

  std::vector<double> whole;
  std::vector<double> chunked;

  // One continuous render.
  for (int i = 0; i < 4096; ++i)
  {
    double left = 0.0;
    double right = 0.0;

    a.next(sa, left, right);
    whole.push_back(left);
  }

  // The same render split into different callback sizes.
  const int sizes[] = {1, 3, 17, 32, 127, 511};

  int count = 0;
  int index = 0;

  while (count < 4096)
  {
    int frames = sizes[index++ % 6];

    if (frames > 4096 - count)
    {
      frames = 4096 - count;
    }

    for (int i = 0; i < frames; ++i)
    {
      double left = 0.0;
      double right = 0.0;

      b.next(sb, left, right);
      chunked.push_back(left);
    }

    count += frames;
  }

  require(na == nb, "chunking source frames");

  for (std::size_t i = 0;
       i < whole.size();
       ++i)
  {
    require(
      whole[i] == chunked[i],
      "chunked output equals continuous output"
    );
  }

  std::puts("PASS chunking");
}

void runImpulse()
{
  lxr::PolyphaseResampler resampler;

  require(
    resampler.configure(48000.0),
    "impulse configuration"
  );

  std::uint64_t index = 0;

  auto source =
    [&](double& left, double& right) noexcept
    {
      left = (index == 0) ? 1.0 : 0.0;
      right = 0.0;
      ++index;
    };

  double energy = 0.0;

  for (int i = 0; i < 256; ++i)
  {
    double left = 0.0;
    double right = 0.0;

    resampler.next(source, left, right);

    require(
      std::isfinite(left) && right == 0.0,
      "impulse stereo isolation"
    );

    energy += left * left;
  }

  require(
    energy > 0.01,
    "impulse response nonzero"
  );

  std::puts("PASS impulse");
}

} // namespace

int main()
{
  for (double rate : {
    44100.0,
    48000.0,
    88200.0,
    96000.0,
    192000.0
  })
  {
    runRate(rate);
  }

  runChunking();
  runImpulse();

  lxr::PolyphaseResampler resampler;

  require(
    !resampler.configure(0.0) &&
    !resampler.configure(1000.0) &&
    !resampler.configure(999999.0),
    "invalid sample rates"
  );

  std::puts("SUMMARY fail=0");
  return 0;
}
