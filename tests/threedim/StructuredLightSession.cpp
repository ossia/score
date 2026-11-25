// StructuredLightSession on its own, on a simulated bench: several projectors
// in one run, a camera with a latency, dropped, frozen or missing frames, a
// gamma, a clipping gain, a projector the camera does not see; abort and
// destruction while computing; the files.

#include "StructuredLightBench.hpp"

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace slbench;
using Catch::Approx;

namespace
{
//! An executor that keeps its jobs until asked to run them.
struct Deferred
{
  std::vector<std::function<void()>> jobs;
  StructuredLightSession::Executor executor()
  {
    return [this](std::function<void()> f) { jobs.push_back(std::move(f)); };
  }
  void runAll()
  {
    auto j = std::move(jobs);
    jobs.clear();
    for(auto& f : j)
      f();
  }
};

bool sawAutoExposure(const RunLog& log, bool value)
{
  return std::any_of(log.events.begin(), log.events.end(), [&](auto& e) {
    return e.autoExposure && *e.autoExposure == value;
  });
}
}

TEST_CASE("A session calibrates several projectors in one run", "[threedim][structuredlight]")
{
  Bench b{4};
  StructuredLightSession session;
  auto s = b.settings();
  double now = 1000.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  CHECK(ev.autoExposure == false);
  REQUIRE(session.images() == 4);
  CHECK_FALSE(session.image(0).idle);
  CHECK(session.image(0).projector == 0);
  CHECK(session.image(1).projector == -1);

  std::vector<int> seen;
  const auto log = run(session, s, b, now, 20000, [&](int) {
    if(session.capturing() && (seen.empty() || seen.back() != session.projector()))
      seen.push_back(session.projector());
  });
  INFO(session.status());
  REQUIRE(log.done);
  CHECK(seen == std::vector<int>{0, 1, 2, 3});
  CHECK(sawAutoExposure(log, true));
  CHECK(session.image(2).idle);
  const auto& results = session.results();
  REQUIRE(results.size() == 4);
  for(int p = 0; p < 4; p++)
  {
    const auto& r = *results[p];
    INFO("projector " << p << ": " << r.status);
    CHECK(r.ok);
    CHECK(r.projectorIndex == p);
    CHECK(r.coverage > 50.f);
    CHECK(medianDepthError(r) < 0.01f);
    CHECK(r.pixels.empty());
  }
}

TEST_CASE("A session waits for a late camera and measures its latency", "[threedim][structuredlight]")
{
  Bench b{1};
  b.latencyMs = 150.;
  b.dropEvery = 3;
  StructuredLightSession session;
  auto s = b.settings();
  s.settleMs = 30;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  const auto log = run(session, s, b, now);
  INFO(session.status());
  REQUIRE(log.done);
  CHECK(session.latency() >= 150.);
  CHECK(session.latency() < 300.);
  const auto& r = *session.results()[0];
  CHECK(r.ok);
  CHECK(medianDepthError(r) < 0.01f);
  CHECK(r.status.find("camera latency") != std::string::npos);
}

TEST_CASE("The latency measured is the camera's, not the settle time", "[threedim][structuredlight]")
{
  Bench b{1};
  StructuredLightSession session;
  auto s = b.settings();
  s.settleMs = 300;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  REQUIRE(run(session, s, b, now).done);
  CHECK(session.latency() < 60.);
  CHECK(session.results()[0]->ok);
}

TEST_CASE("A session stops when the camera delivers nothing", "[threedim][structuredlight]")
{
  Bench b{1};
  b.stopAt = 0.;
  StructuredLightSession session;
  auto s = b.settings();
  s.timeoutMs = 300.;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  const auto log = run(session, s, b, now, 200);
  CHECK_FALSE(session.running());
  CHECK_FALSE(log.done);
  CHECK(session.status().starts_with("Failed: no camera frame"));
  CHECK(sawAutoExposure(log, true));
  CHECK(now < 1000.);
}

TEST_CASE("A frozen camera fails its projector, not the next one", "[threedim][structuredlight]")
{
  Bench b{2};
  StructuredLightSession session;
  auto s = b.settings();
  s.timeoutMs = 300.;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  bool frozen = false, thawed = false;
  const auto log = run(session, s, b, now, 20000, [&](int) {
    if(!frozen && session.projector() == 0 && session.sequence().count() > 0
       && session.progress() > 0.1f)
    {
      frozen = true;
      b.freezeAt = now;
    }
    if(frozen && !thawed && session.projector() == 1)
    {
      thawed = true;
      b.freezeAt = -1.;
    }
  });
  INFO(session.status());
  REQUIRE(log.done);
  REQUIRE(session.results().size() == 2);
  CHECK_FALSE(session.results()[0]->ok);
  CHECK(session.results()[0]->status.find("stopped changing") != std::string::npos);
  CHECK(session.results()[1]->ok);
}

TEST_CASE("A projector the camera does not see fails alone", "[threedim][structuredlight]")
{
  Bench b{3};
  b.hidden = {1};
  StructuredLightSession session;
  auto s = b.settings();
  s.timeoutMs = 300.;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  const auto log = run(session, s, b, now);
  INFO(session.status());
  REQUIRE(log.done);
  const auto& r = session.results();
  REQUIRE(r.size() == 3);
  CHECK(r[0]->ok);
  CHECK_FALSE(r[1]->ok);
  CHECK(r[1]->status.find("does not see projector 2") != std::string::npos);
  CHECK(r[2]->ok);
  CHECK(session.status().find("P2: Failed") != std::string::npos);
}

TEST_CASE("Captures are linearised with the camera gamma before decoding", "[threedim][structuredlight]")
{
  auto phaseError = [](float cameraGamma) {
    Bench b{1};
    b.encodeGamma = 2.2f;
    b.gain = 0.9f;
    StructuredLightSession session;
    auto s = b.settings();
    s.cameraGamma = cameraGamma;
    s.projectorGamma = 1.f;
    s.phasePeriod = 12;
    s.keepPixels = true;
    double now = 0.;
    StructuredLightSession::Events ev;
    REQUIRE(session.start(s, {}, now, ev));
    run(session, s, b, now);
    REQUIRE(session.results().size() == 1);
    const auto& r = *session.results()[0];
    INFO(r.status);
    REQUIRE(r.ok);
    std::vector<double> err;
    for(int i = 0; i < CW * CH; i++)
    {
      const auto& p = r.pixels[i];
      const int q = b.seen[0][i];
      if(!p.valid || !p.phaseU || q < 0)
        continue;
      err.push_back(std::abs(p.u - float(q % PW)));
    }
    REQUIRE(err.size() > 1000);
    std::nth_element(err.begin(), err.begin() + err.size() / 2, err.end());
    return err[err.size() / 2];
  };
  const double raw = phaseError(1.f), linear = phaseError(2.2f);
  INFO("median phase error: raw codes " << raw << " px, linearised " << linear << " px");
  CHECK(linear < 0.5 * raw);
}

TEST_CASE("Clipped camera pixels are kept out of the phase", "[threedim][structuredlight]")
{
  Bench b{1};
  b.gain = 2.f;
  StructuredLightSession session;
  auto s = b.settings();
  s.keepPixels = true;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  run(session, s, b, now);
  REQUIRE(session.results().size() == 1);
  const auto& r = *session.results()[0];
  INFO(r.status);
  REQUIRE(r.ok);
  int saturated = 0, phased = 0;
  for(const auto& p : r.pixels)
    if(p.valid && p.saturated)
    {
      saturated++;
      phased += p.phaseU || p.phaseV;
    }
  CHECK(saturated > 1000);
  CHECK(phased == 0);
  CHECK(medianDepthError(r) < 0.02f);
}

TEST_CASE("Abort and destruction while computing drop the results", "[threedim][structuredlight]")
{
  Bench b{1};
  auto s = b.settings();
  Deferred exec;
  {
    StructuredLightSession session;
    session.setExecutor(exec.executor());
    double now = 0.;
    StructuredLightSession::Events ev;
    REQUIRE(session.start(s, {}, now, ev));
    run(session, s, b, now, 20000, {}, true);
    CHECK(session.running());
    CHECK(session.status() == "Decoding");
    REQUIRE(exec.jobs.size() == 1);
    session.abort(s, ev);
    exec.runAll();
    StructuredLightSession::Frame none;
    session.tick(s, now + 20., none, ev);
    CHECK(session.results().empty());
    CHECK(session.status() == "Aborted");

    REQUIRE(session.start(s, {}, now, ev));
    run(session, s, b, now, 20000, {}, true);
    REQUIRE(exec.jobs.size() == 1);
  }
  exec.runAll();
}

TEST_CASE("A saved run loads back, with the files it lists", "[threedim][structuredlight][files]")
{
  QTemporaryDir dir;
  Bench b{2};
  StructuredLightSession session;
  auto s = b.settings();
  double now = 0.;
  StructuredLightSession::Events ev;
  StructuredLightSession::Run r;
  r.autoSave = dir.filePath("room.json");
  REQUIRE(session.start(s, r, now, ev));
  run(session, s, b, now);
  StructuredLightSession::Frame none;
  session.tick(s, now + 20., none, ev);
  INFO(session.status());
  REQUIRE(session.status().starts_with("Saved"));
  const QString p1 = dir.filePath("room_p1");
  CHECK(Gfx::calibrationListing(p1)->contains("room_p1.xyz.pfm"));

  const auto c = Threedim::loadCalibration(p1);
  REQUIRE(c.ok());
  CHECK(c.projectorIndex == 1);
  REQUIRE(c.camera);
  CHECK(c.camera->width == CW);
  CHECK(c.json["sequence"].toObject()["patterns"].toInt() == session.sequence().count());

  StructuredLightSession other;
  other.load(p1 + ".xyz.pfm");
  other.tick(s, 0., none, ev);
  REQUIRE(other.results().size() == 1);
  const auto& loaded = *other.results()[0];
  INFO(loaded.status);
  CHECK(loaded.ok);
  CHECK(loaded.fromFile);
  CHECK(loaded.projectorIndex == 1);
  CHECK(loaded.xyz == session.results()[1]->xyz);
  CHECK(loaded.intrinsics == session.results()[1]->intrinsics);
  CHECK(loaded.camWidth == CW);

  other.save(dir.filePath("again"));
  other.tick(s, 0., none, ev);
  CHECK(other.status().find("not saved again") != std::string::npos);
}

TEST_CASE("A run restores the camera's auto exposure when it ends", "[threedim][structuredlight]")
{
  Bench b{1};
  StructuredLightSession session;
  auto s = b.settings();
  s.captureExposure = 10.f;
  s.autoExposureAfter = false;
  s.exposureSettleMs = 50.;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  CHECK(ev.autoExposure == false);
  CHECK(ev.exposure == 10.f);
  const auto log = run(session, s, b, now);
  REQUIRE(log.done);
  CHECK_FALSE(sawAutoExposure(log, true));

  s.autoExposureAfter = true;
  ev = {};
  REQUIRE(session.start(s, {}, now, ev));
  session.abort(s, ev);
  CHECK(ev.autoExposure == true);
}

TEST_CASE("A session reads the camera only while it captures", "[threedim][structuredlight]")
{
  Bench b{1};
  StructuredLightSession session;
  auto s = b.settings();
  s.settleMs = 100;
  double now = 0.;
  StructuredLightSession::Events ev;
  REQUIRE(session.start(s, {}, now, ev));
  StructuredLightSession::Frame none;
  session.tick(s, 50., none, ev);
  CHECK_FALSE(session.wantsCamera());
  session.tick(s, 120., none, ev);
  CHECK(session.wantsCamera());
  CHECK(session.wantsPositions());
}

TEST_CASE("Camera intrinsics of another camera size are refused", "[threedim][structuredlight]")
{
  Bench b{1};
  auto s = b.settings();
  s.mode = StructuredLightSession::ViewerCamera;
  auto runWith = [&](std::array<float, 4> k) {
    StructuredLightSession session;
    s.cameraIntrinsics = k;
    double now = 0.;
    StructuredLightSession::Events ev;
    REQUIRE(session.start(s, {}, now, ev));
    run(session, s, b, now);
    REQUIRE(session.results().size() == 1);
    return *session.results()[0];
  };
  const auto good = runWith({float(b.fx), float(b.fy), float(b.cx), float(b.cy)});
  INFO(good.status);
  CHECK(good.ok);
  const auto wrong = runWith({float(b.fx), float(b.fy), 2.f * CW, float(b.cy)});
  INFO(wrong.status);
  CHECK_FALSE(wrong.ok);
  CHECK(wrong.status.find("not those of a 480 x 200 camera") != std::string::npos);
}
