#include <Gfx/CameraControls.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <thread>

using Gfx::CameraControlBackend;
using Gfx::CameraControls;
using Catch::Approx;

namespace
{
struct FakeBackend final : CameraControlBackend
{
  std::vector<Control> controls{
      {"exposure", "auto_exposure", 488.28125, 250000., true},
      {"gain", "", 0., 255., false},
  };
  std::vector<State> state{{31250., true}, {128., false}};
  struct Set
  {
    std::size_t control;
    double value;
    bool automatic;
  };
  std::vector<Set> sets;
  int gets = 0;
  bool reachable = true;
  bool closed = false;

  std::vector<Control> open() override { return controls; }
  std::optional<State> get(std::size_t i) override
  {
    gets++;
    if(!reachable)
      return std::nullopt;
    return state[i];
  }
  bool set(std::size_t i, double value, bool automatic) override
  {
    sets.push_back({i, value, automatic});
    if(!reachable)
      return false;
    state[i] = {std::clamp(value, controls[i].min, controls[i].max), automatic};
    return true;
  }
  void close() override { closed = true; }
};

struct Rig
{
  ossia::net::generic_device dev{
      std::make_unique<ossia::net::multiplex_protocol>(), "camera"};
  FakeBackend* backend{};
  std::unique_ptr<CameraControls> controls;

  Rig()
  {
    auto b = std::make_unique<FakeBackend>();
    backend = b.get();
    controls = std::make_unique<CameraControls>(std::move(b), false);
    controls->setup(dev.get_root_node());
  }

  ossia::net::parameter_base* param(const char* name)
  {
    auto* n = ossia::net::find_node(
        dev.get_root_node(), std::string("/controls/color/") + name);
    return n ? n->get_parameter() : nullptr;
  }
};

const auto t0 = CameraControls::clock::now();
}

TEST_CASE("exposure goes to the nearest DirectShow stop, within the device range",
          "[gfx][camera]")
{
  using Gfx::dshow::exposureMicroseconds;
  using Gfx::dshow::exposureStop;
  REQUIRE(exposureStop(100000., -11, -2));
  CHECK(exposureMicroseconds(*exposureStop(100000., -11, -2)) == Approx(125000.));
  CHECK(exposureMicroseconds(*exposureStop(5e6, -11, -2)) == Approx(250000.));
  CHECK(*exposureStop(1., -11, -2) == -11);
  CHECK(*exposureStop(62500., -11, -2) == -4);
  CHECK_FALSE(exposureStop(0., -11, -2));
  CHECK_FALSE(exposureStop(-1., -11, -2));
  CHECK_FALSE(exposureStop(std::numeric_limits<double>::infinity(), -11, -2));
  CHECK_FALSE(exposureStop(std::numeric_limits<double>::quiet_NaN(), -11, -2));
}

TEST_CASE("a control is set again only when the device moved away from what was written",
          "[gfx][camera]")
{
  using W = CameraControls::Wanted;
  CHECK_FALSE(CameraControls::needsReapply(W{}, {5., true}, true));
  const W manual{true, 62500., false, 62500.};
  CHECK_FALSE(CameraControls::needsReapply(manual, {62500., false}, true));
  CHECK(CameraControls::needsReapply(manual, {31250., false}, true));
  CHECK(CameraControls::needsReapply(manual, {62500., true}, true));
  CHECK_FALSE(CameraControls::needsReapply(W{true, 0., false, 0.}, {0., true}, false));
  CHECK(CameraControls::needsReapply(W{true, 0., false, 0.}, {255., false}, false));
  const W automatic{true, 31250., true, 31250.};
  CHECK_FALSE(CameraControls::needsReapply(automatic, {7000., true}, true));
  CHECK(CameraControls::needsReapply(automatic, {7000., false}, true));
}

TEST_CASE("camera controls are published under /controls/color with the device's ranges",
          "[gfx][camera]")
{
  Rig r;
  auto* exposure = r.param("exposure");
  auto* autoExposure = r.param("auto_exposure");
  auto* gain = r.param("gain");
  REQUIRE(exposure);
  REQUIRE(autoExposure);
  REQUIRE(gain);
  CHECK_FALSE(r.param("auto_gain"));
  CHECK_FALSE(r.param("focus"));

  CHECK(exposure->get_value_type() == ossia::val_type::FLOAT);
  CHECK(gain->get_value_type() == ossia::val_type::INT);
  CHECK(autoExposure->get_value_type() == ossia::val_type::BOOL);
  CHECK(ossia::convert<float>(exposure->value()) == Approx(31250.f));
  CHECK(ossia::convert<int>(gain->value()) == 128);
  CHECK(ossia::convert<bool>(autoExposure->value()));
  CHECK(ossia::convert<float>(ossia::get_min(exposure->get_domain())) == Approx(488.28125f));
  CHECK(ossia::convert<int>(ossia::get_max(gain->get_domain())) == 255);
}

TEST_CASE("writes are merged: the camera gets the latest value once", "[gfx][camera]")
{
  Rig r;
  for(int i = 0; i < 100; i++)
    CHECK(r.controls->push(*r.param("gain"), i));
  r.controls->step(t0);
  REQUIRE(r.backend->sets.size() == 1);
  CHECK(r.backend->sets[0].control == 1);
  CHECK(r.backend->sets[0].value == 99.);
  CHECK(ossia::convert<int>(r.param("gain")->value()) == 99);
}

TEST_CASE("a value write sets manual mode, a mode write keeps the value", "[gfx][camera]")
{
  Rig r;
  r.controls->push(*r.param("exposure"), 62500.f);
  r.controls->step(t0);
  REQUIRE(r.backend->sets.size() == 1);
  CHECK_FALSE(r.backend->sets[0].automatic);
  CHECK_FALSE(ossia::convert<bool>(r.param("auto_exposure")->value()));

  r.controls->push(*r.param("auto_exposure"), true);
  r.controls->step(t0);
  r.controls->push(*r.param("auto_exposure"), false);
  r.controls->step(t0);
  REQUIRE(r.backend->sets.size() == 3);
  CHECK(r.backend->sets[1].automatic);
  CHECK_FALSE(r.backend->sets[2].automatic);
  CHECK(r.backend->sets[2].value == Approx(62500.));
}

TEST_CASE("non-finite writes are refused", "[gfx][camera]")
{
  Rig r;
  CHECK_FALSE(r.controls->push(*r.param("exposure"), std::numeric_limits<float>::infinity()));
  CHECK_FALSE(r.controls->push(*r.param("exposure"), std::numeric_limits<float>::quiet_NaN()));
  r.controls->step(t0);
  CHECK(r.backend->sets.empty());
}

TEST_CASE("after the capture starts, a control the camera reset is set again, for a while",
          "[gfx][camera]")
{
  Rig r;
  r.controls->push(*r.param("exposure"), 62500.f);
  r.controls->push(*r.param("gain"), 0);
  r.controls->step(t0);
  REQUIRE(r.backend->sets.size() == 2);

  r.controls->streamStarted();
  r.backend->state[0] = {31250., true};
  r.backend->state[1] = {255., false};
  const auto now = CameraControls::clock::now();
  r.controls->step(now);
  REQUIRE(r.backend->sets.size() == 4);
  CHECK(r.backend->state[0].value == 62500.);
  CHECK_FALSE(r.backend->state[0].automatic);
  CHECK(r.backend->state[1].value == 0.);

  r.controls->step(now + CameraControls::enforce_interval);
  CHECK(r.backend->sets.size() == 4);

  r.backend->state[1] = {255., false};
  r.controls->step(now + CameraControls::enforce_duration + CameraControls::poll_interval);
  r.controls->step(
      now + CameraControls::enforce_duration + 2 * CameraControls::poll_interval);
  CHECK(r.backend->sets.size() == 4);
}

TEST_CASE("a control without auto mode is not set again for its flags", "[gfx][camera]")
{
  Rig r;
  r.controls->push(*r.param("gain"), 10);
  r.controls->step(t0);
  r.controls->streamStarted();
  r.backend->state[1] = {10., true};
  auto now = CameraControls::clock::now();
  for(int i = 0; i < 10; i++)
    r.controls->step(now + i * CameraControls::enforce_interval);
  CHECK(r.backend->sets.size() == 1);
}

TEST_CASE("controls on auto are polled, the others are not", "[gfx][camera]")
{
  Rig r;
  r.backend->state[0] = {1000., true};
  r.controls->step(t0);
  CHECK(ossia::convert<float>(r.param("exposure")->value()) == Approx(1000.f));

  r.backend->gets = 0;
  r.controls->step(t0 + CameraControls::poll_interval);
  CHECK(r.backend->gets == 1);
}

TEST_CASE("a camera that stops answering is no longer polled", "[gfx][camera]")
{
  Rig r;
  r.backend->reachable = false;
  for(int i = 0; i < 10; i++)
    r.controls->step(t0 + i * CameraControls::poll_interval);
  CHECK(r.backend->gets == 3 + 2);
}

TEST_CASE("a stopped camera takes no write and closes its backend", "[gfx][camera]")
{
  Rig r;
  r.controls->stop();
  CHECK(r.backend->closed);
  CHECK_FALSE(r.controls->push(*r.param("gain"), 3));
}

TEST_CASE("a threaded camera control applies writes on its own thread", "[gfx][camera]")
{
  ossia::net::generic_device dev{
      std::make_unique<ossia::net::multiplex_protocol>(), "camera"};
  auto b = std::make_unique<FakeBackend>();
  auto* backend = b.get();
  CameraControls controls{std::move(b)};
  REQUIRE_FALSE(controls.empty());
  controls.setup(dev.get_root_node());
  auto* gain
      = ossia::net::find_node(dev.get_root_node(), "/controls/color/gain")->get_parameter();
  controls.push(*gain, 42);
  for(int i = 0; i < 200 && ossia::convert<int>(gain->value()) != 42; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  controls.stop();
  CHECK(ossia::convert<int>(gain->value()) == 42);
  CHECK(backend->closed);
}
