// The capture of one shown pattern: settle, average, give up, and tell a
// frozen or a missing camera apart.

#include <Threedim/PatternCapture.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using Threedim::CameraFrame;
using Threedim::PatternCapture;
using State = PatternCapture::State;
using Catch::Approx;

namespace
{
std::vector<float> uniform(int w, int h, float v)
{
  std::vector<float> f(std::size_t(w) * h * 4, v);
  for(std::size_t i = 3; i < f.size(); i += 4)
    f[i] = 1.f;
  return f;
}
}

TEST_CASE("A pattern capture settles, then averages the frames that follow", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  PatternCapture::Options o;
  o.settleMs = 100.;
  o.frames = 2;
  o.rgb = true;
  c.begin(0., o);
  auto a = uniform(4, 2, 0.2f), b = uniform(4, 2, 0.4f);
  CHECK(c.feed(50., {a.data(), 4, 2}) == State::Settling);
  CHECK_FALSE(c.wantsFrame());
  CHECK(c.feed(100., {a.data(), 4, 2}) == State::Capturing);
  CHECK(c.accepted());
  CHECK(c.wantsFrame());
  CHECK(c.feed(110., {b.data(), 4, 2}) == State::Capturing);
  CHECK_FALSE(c.accepted());
  CHECK(c.feed(140., {b.data(), 4, 2}) == State::Done);
  CHECK(c.luminance().size() == 8);
  CHECK(c.luminance()[0] == Approx(0.3f));
  CHECK(c.rgb()[2] == Approx(0.3f));
  CHECK(c.mean() == Approx(0.3f));
  CHECK(c.latency() == Approx(100.));
}

TEST_CASE("A pattern capture takes a frame per new serial", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  PatternCapture::Options o;
  o.settleMs = 0.;
  o.frames = 2;
  c.begin(0., o);
  auto a = uniform(2, 2, 0.5f);
  CHECK(c.feed(0., {a.data(), 2, 2, 7}) == State::Capturing);
  CHECK(c.accepted());
  CHECK(c.feed(100., {a.data(), 2, 2, 7}) == State::Capturing);
  CHECK_FALSE(c.accepted());
  CHECK(c.feed(101., {a.data(), 2, 2, 8}) == State::Done);
}

TEST_CASE("A pattern capture fails when no frame comes", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  PatternCapture::Options o;
  o.settleMs = 10.;
  o.timeoutMs = 200.;
  c.begin(0., o);
  CHECK(c.feed(20., {}) == State::Capturing);
  CHECK(c.feed(200., {}) == State::Capturing);
  CHECK(c.feed(211., {}) == State::NoFrame);
  CHECK(c.failed());
}

TEST_CASE("A frozen camera does not pass for a new pattern", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  PatternCapture::Options o;
  o.settleMs = 0.;
  o.timeoutMs = 300.;
  o.requireChange = true;
  auto a = uniform(8, 8, 0.7f), b = uniform(8, 8, 0.1f);
  c.begin(0., o);
  REQUIRE(c.feed(0., {a.data(), 8, 8}) == State::Done);

  c.begin(100., o);
  CHECK(c.feed(100., {a.data(), 8, 8}) == State::Capturing);
  CHECK_FALSE(c.accepted());
  CHECK(c.feed(250., {a.data(), 8, 8}) == State::Capturing);
  CHECK(c.feed(250. + 40., {b.data(), 8, 8}) == State::Done);
  CHECK(c.latency() == Approx(190.));

  c.begin(1000., o);
  for(double t = 1000.; t < 1400.; t += 40.)
    c.feed(t, {b.data(), 8, 8});
  CHECK(c.state() == State::Unchanged);
}

TEST_CASE("A capture waiting for a darker frame measures the latency", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  PatternCapture::Options o;
  o.settleMs = 0.;
  o.darkerThan = 0.3f;
  auto bright = uniform(4, 4, 0.8f), dark = uniform(4, 4, 0.05f);
  c.begin(0., o);
  CHECK(c.feed(0., {bright.data(), 4, 4}) == State::Capturing);
  CHECK(c.feed(40., {bright.data(), 4, 4}) == State::Capturing);
  CHECK(c.feed(160., {dark.data(), 4, 4}) == State::Done);
  CHECK(c.latency() == Approx(160.));
}

TEST_CASE("A pattern capture linearises, flags clipped pixels and a size change", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  const auto table = PatternCapture::gammaTable(2.f, 1025);
  PatternCapture::Options o;
  o.settleMs = 0.;
  o.linearise = &table;
  o.clipLevel = 0.98f;
  auto a = uniform(2, 1, 0.5f);
  a[4] = 1.f;
  c.begin(0., o);
  REQUIRE(c.feed(0., {a.data(), 2, 1}) == State::Done);
  CHECK(c.luminance()[0] == Approx(0.25f).margin(1e-4));
  CHECK(c.clipped() == std::vector<uint8_t>{0, 1});

  auto b = uniform(3, 1, 0.5f);
  c.begin(10., o);
  CHECK(c.feed(50., {b.data(), 3, 1}) == State::SizeChanged);
}

TEST_CASE("Frames that never change time out as unchanged, not as missing", "[threedim][capture]")
{
  PatternCapture c;
  c.reset();
  PatternCapture::Options o;
  o.settleMs = 0.;
  o.timeoutMs = 200.;
  o.requireChange = true;
  auto a = uniform(4, 4, 0.5f);
  c.begin(0., o);
  REQUIRE(c.feed(0., {a.data(), 4, 4}) == State::Done);
  c.begin(10., o);
  for(double t = 10.; t < 400. && !c.failed(); t += 16.7)
    c.feed(t, {a.data(), 4, 4});
  CHECK(c.state() == State::Unchanged);
}
