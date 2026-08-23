// Unit tests for the generic point tracker: the ossia::point_tracker core, the
// avnd_tools::PointTracker2D/3D processes built on it, and the ossia::value
// round-trip of their port types.
#include <ossia/math/point_tracker.hpp>
#include <ossia/network/value/value.hpp>

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>

#include <AvndProcesses/PointTracker.hpp>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <vector>

using T2 = ossia::point_tracker<2>;
using C2 = ossia::point_tracker_config<2>;
using D2 = ossia::point_detection<2>;

namespace
{
C2 base_cfg()
{
  C2 c;
  c.max_speed = 2.f;
  c.gate = ossia::track_motion_gate::max_speed;
  c.confirm_time = 0.100f;
  c.confirm_hits = 3;
  c.confirm_window = 5;
  c.instant_confirm = 2.f; // disabled unless a test wants it
  c.coast_time = 0.5f;
  c.revive = true;
  c.revive_time = 2.f;
  c.smooth = false; // most core tests reason about the raw Kalman state
  return c;
}

const ossia::point_track<2>* find_track(const T2& t, std::int32_t id)
{
  for(const auto& tr : t.tracks())
    if(tr.id == id)
      return &tr;
  return nullptr;
}
}

TEST_CASE("association: two objects crossing keep their ids", "[point_tracker]")
{
  T2 trk;
  auto cfg = base_cfg();
  cfg.dir_weight = 0.2f;
  trk.configure(cfg);

  const float dt = 1.f / 30.f;
  std::int32_t idA = -1, idB = -1;
  for(int f = 0; f < 60; f++)
  {
    const float t = f * dt; // 2 s total: they cross at t = 1
    std::vector<D2> dets{
        {{0.1f + 0.4f * t, 0.49f}, 0.8f},
        {{0.9f - 0.4f * t, 0.51f}, 0.8f},
    };
    const auto& ids = trk.update(dets, dt);
    REQUIRE(ids.size() == 2);
    if(f == 0)
    {
      idA = ids[0];
      idB = ids[1];
      REQUIRE(idA > 0);
      REQUIRE(idB > 0);
      REQUIRE(idA != idB);
    }
    else
    {
      // Identity must survive the crossing: detection 0 is always object A.
      REQUIRE(ids[0] == idA);
      REQUIRE(ids[1] == idB);
    }
  }
}

TEST_CASE("id stability across a one-frame dropout", "[point_tracker]")
{
  T2 trk;
  trk.configure(base_cfg());
  const float dt = 1.f / 30.f;

  std::int32_t id = -1;
  int exits = 0, enters = 0;
  for(int f = 0; f < 30; f++)
  {
    std::vector<D2> dets;
    if(f != 15) // one-frame dropout
      dets.push_back({{0.5f + 0.002f * f, 0.5f}, 0.8f});
    const auto& ids = trk.update(dets, dt);
    enters += trk.events().entered.size();
    exits += trk.events().exited.size();
    if(f == 0)
      id = ids[0];
    else if(f != 15)
      REQUIRE(ids[0] == id);
  }
  REQUIRE(enters == 1);
  REQUIRE(exits == 0);
}

TEST_CASE("two-stage association sustains a confidence dip", "[point_tracker]")
{
  const float dt = 1.f / 30.f;
  for(const bool two_stage : {true, false})
  {
    T2 trk;
    auto cfg = base_cfg();
    cfg.two_stage = two_stage;
    trk.configure(cfg);

    // Confirm with high-confidence frames...
    std::int32_t id = -1;
    for(int f = 0; f < 10; f++)
    {
      std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
      id = trk.update(dets, dt)[0];
    }
    REQUIRE(find_track(trk, id)->state == ossia::track_state::confirmed);

    // ...then dip to low confidence (0.3: below high 0.5, above low 0.1).
    for(int f = 0; f < 3; f++)
    {
      std::vector<D2> dets{{{0.5f, 0.5f}, 0.3f}};
      const auto& ids = trk.update(dets, dt);
      if(two_stage)
      {
        // ByteTrack: the low-confidence detection sustains the track.
        REQUIRE(ids[0] == id);
        REQUIRE(find_track(trk, id)->time_since_seen == 0.f);
      }
      else
      {
        // Without the second stage the detection is ignored; the track coasts.
        REQUIRE(ids[0] == -1);
        REQUIRE(find_track(trk, id)->time_since_seen > 0.f);
      }
    }
  }
}

TEST_CASE(
    "TCM: confidence trend keeps ids through an ambiguous crossing",
    "[point_tracker]")
{
  // Hybrid-SORT Tracklet Confidence Modeling. Two objects cross exactly; the
  // occluded one's confidence ramps down into the second (low-confidence)
  // ByteTrack stage near the overlap and recovers after. At the coincidence
  // frame the position costs tie, so identity has to come from the
  // confidence trend: the track whose Kalman confidence estimate is still
  // high takes the high-confidence detection.
  T2 trk;
  auto cfg = base_cfg();
  cfg.dir_weight = 0.f; // isolate the confidence cue from the direction cue
  cfg.conf_weight = 1.f;
  trk.configure(cfg);

  const float dt = 1.f / 30.f;
  std::int32_t idA = -1, idB = -1;
  bool b_dipped_low = false;
  for(int f = 0; f < 90; f++)
  {
    const float t = f * dt; // 3 s: exact coincidence at t = 1.5 (frame 45)
    const float ax = 0.3f + 0.1f * t;
    const float bx = 0.6f - 0.1f * t;
    const float occ = std::clamp((0.1f - std::abs(ax - bx)) / 0.08f, 0.f, 1.f);
    const float conf_b = 0.9f - 0.6f * occ; // sinks to 0.3: the low stage
    b_dipped_low = b_dipped_low || conf_b < cfg.high_conf;

    std::vector<D2> dets{
        {{ax, 0.5f}, 0.9f},
        {{bx, 0.5f}, conf_b},
    };
    const auto& ids = trk.update(dets, dt);
    if(f == 0)
    {
      idA = ids[0];
      idB = ids[1];
      REQUIRE(idA > 0);
      REQUIRE(idB > 0);
    }
    else
    {
      // Identity must survive the crossing AND the confidence dip: the
      // low-confidence detections keep sustaining B through the second
      // stage, where TCM predicts by linear extrapolation.
      REQUIRE(ids[0] == idA);
      REQUIRE(ids[1] == idB);
    }
  }
  REQUIRE(b_dipped_low); // the scenario did route B through the low stage

  // The confidence Kalman must have followed the trends.
  REQUIRE(find_track(trk, idA)->conf_kf.p == Catch::Approx(0.9f).margin(0.05));
  REQUIRE(find_track(trk, idB)->confidence > 0.8f); // recovered after the dip
}

TEST_CASE("confirmation timing is in milliseconds, not frames", "[point_tracker]")
{
  // confirm_time = 100 ms, M = 3 of N = 5: confirmation should land at
  // max(100 ms, the time of the 3rd hit) regardless of the frame rate.
  const struct
  {
    float fps;
    double expected_s;
  } cases[] = {
      {30.f, 0.100},  // age reaches 100 ms on frame 3
      {60.f, 0.100},  // frame 6
      {144.f, 0.1042}, // first frame at or after 100 ms: 15/144
      {10.f, 0.200},  // 3rd hit only happens at 200 ms
  };
  for(const auto& c : cases)
  {
    T2 trk;
    trk.configure(base_cfg());
    const float dt = 1.f / c.fps;
    double confirmed_at = -1.;
    for(int f = 0; f < 200 && confirmed_at < 0; f++)
    {
      std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
      trk.update(dets, dt);
      if(!trk.events().confirmed.empty())
        confirmed_at = trk.now();
    }
    INFO("fps = " << c.fps);
    REQUIRE(confirmed_at == Catch::Approx(c.expected_s).margin(1.5 * dt));
  }
}

TEST_CASE("instant confirmation above the threshold", "[point_tracker]")
{
  T2 trk;
  auto cfg = base_cfg();
  cfg.instant_confirm = 0.9f;
  trk.configure(cfg);
  std::vector<D2> dets{{{0.5f, 0.5f}, 0.95f}};
  const auto& ids = trk.update(dets, 1.f / 30.f);
  REQUIRE(trk.events().entered.size() == 1);
  REQUIRE(trk.events().confirmed.size() == 1);
  REQUIRE(find_track(trk, ids[0])->state == ossia::track_state::confirmed);
}

TEST_CASE("coast, revive, expire with exactly-once exit", "[point_tracker]")
{
  T2 trk;
  auto cfg = base_cfg();
  cfg.coast_time = 0.5f;
  cfg.revive = true;
  cfg.revive_time = 2.f;
  trk.configure(cfg);
  const float dt = 1.f / 30.f;

  int entered = 0, exited = 0, revived = 0;
  auto count = [&] {
    entered += trk.events().entered.size();
    exited += trk.events().exited.size();
    revived += trk.events().revived.size();
  };

  // Confirm a static object.
  std::int32_t id = -1;
  for(int f = 0; f < 10; f++)
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
    id = trk.update(dets, dt)[0];
    count();
  }
  REQUIRE(find_track(trk, id)->state == ossia::track_state::confirmed);

  // Silence for 1 s: coasting, then lost (not emitted), but not yet expired.
  for(int f = 0; f < 30; f++)
  {
    trk.advance(dt);
    count();
  }
  {
    const auto* t = find_track(trk, id);
    REQUIRE(t != nullptr);
    REQUIRE(t->state == ossia::track_state::lost);
    REQUIRE(!t->emitted(true));
    REQUIRE(exited == 0);
  }

  // A detection where the object was: revived, same id, exactly one event.
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
    const auto& ids = trk.update(dets, dt);
    count();
    REQUIRE(ids[0] == id);
    REQUIRE(revived == 1);
    const auto* t = find_track(trk, id);
    REQUIRE(t->state == ossia::track_state::revived);
    REQUIRE(t->reacquired);
    // ORU: the re-update must not leave a lurching velocity on a static object.
    const auto v = t->velocity();
    REQUIRE(std::abs(v[0]) < 0.5f);
    REQUIRE(std::abs(v[1]) < 0.5f);
  }

  // A few more frames: back to confirmed.
  for(int f = 0; f < 5; f++)
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
    trk.update(dets, dt);
    count();
  }
  REQUIRE(find_track(trk, id)->state == ossia::track_state::confirmed);

  // Full silence until well past coast + revive: exactly one exit, ever.
  for(int f = 0; f < 100; f++)
  {
    trk.advance(dt);
    count();
  }
  REQUIRE(trk.tracks().empty());
  REQUIRE(exited == 1);
  REQUIRE(entered == 1);
  REQUIRE(revived == 1);
}

TEST_CASE("exit fires exactly once with revive off", "[point_tracker]")
{
  T2 trk;
  auto cfg = base_cfg();
  cfg.revive = false;
  cfg.coast_time = 0.2f;
  trk.configure(cfg);
  const float dt = 1.f / 30.f;

  for(int f = 0; f < 10; f++)
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
    trk.update(dets, dt);
  }
  int exited = 0;
  for(int f = 0; f < 60; f++)
  {
    trk.advance(dt);
    exited += trk.events().exited.size();
  }
  REQUIRE(exited == 1);
  REQUIRE(trk.tracks().empty());
}

TEST_CASE("variable dt: same motion at 30/60/144 fps and irregular", "[point_tracker]")
{
  // A point moving at 0.5 units/s for 1 s. Whatever the frame spacing, the
  // tracker must end with one confirmed track around x = 0.6 moving at
  // ~0.5 units/s. This is the regression test for the dt=1 class of bug: a
  // frame-based filter would report a velocity 30/60/144x off depending on the
  // rate.
  auto run = [](const std::vector<float>& dts) {
    T2 trk;
    trk.configure(base_cfg());
    float t = 0.f;
    for(const float dt : dts)
    {
      t += dt;
      std::vector<D2> dets{{{0.1f + 0.5f * t, 0.5f}, 0.8f}};
      trk.update(dets, dt);
    }
    REQUIRE(trk.tracks().size() == 1);
    const auto& tr = trk.tracks()[0];
    REQUIRE(tr.state == ossia::track_state::confirmed);
    const auto p = tr.position();
    const auto v = tr.velocity();
    REQUIRE(p[0] == Catch::Approx(0.6).margin(0.01));
    REQUIRE(p[1] == Catch::Approx(0.5).margin(0.01));
    REQUIRE(v[0] == Catch::Approx(0.5).margin(0.1));
    REQUIRE(std::abs(v[1]) < 0.05f);
  };

  for(const float fps : {30.f, 60.f, 144.f})
  {
    INFO("fps = " << fps);
    run(std::vector<float>(std::size_t(fps), 1.f / fps));
  }

  // Irregular spacing: alternating 5 ms / 61.67 ms, still summing to 1 s.
  {
    std::vector<float> dts;
    for(int i = 0; i < 15; i++)
    {
      dts.push_back(0.005f);
      dts.push_back(0.06166f);
    }
    INFO("irregular dt");
    run(dts);
  }
}

TEST_CASE("ids are never reused; slots recycle through the quarantine hold", "[point_tracker]")
{
  T2 trk;
  auto cfg = base_cfg();
  cfg.instant_confirm = 0.9f;
  cfg.slot_count = 2;
  cfg.slot_hold_time = 0.25f;
  cfg.coast_time = 0.2f;
  cfg.revive = false;
  trk.configure(cfg);
  const float dt = 1.f / 30.f;

  // A and B fill both slots.
  std::int32_t idA = -1, idB = -1;
  for(int f = 0; f < 3; f++)
  {
    std::vector<D2> dets{{{0.2f, 0.2f}, 0.95f}, {{0.8f, 0.8f}, 0.95f}};
    const auto& ids = trk.update(dets, dt);
    idA = ids[0];
    idB = ids[1];
  }
  REQUIRE(idA == 1);
  REQUIRE(idB == 2);
  REQUIRE(find_track(trk, idA)->slot == 0);
  REQUIRE(find_track(trk, idB)->slot == 1);

  // A disappears; B stays. A expires after coast_time and vacates slot 0.
  int f_exit = -1;
  for(int f = 0; f < 30 && f_exit < 0; f++)
  {
    std::vector<D2> dets{{{0.8f, 0.8f}, 0.95f}};
    trk.update(dets, dt);
    if(!trk.events().exited.empty())
      f_exit = f;
  }
  REQUIRE(f_exit >= 0);

  // C appears immediately: instant-confirmed, but slot 0 is quarantined.
  std::int32_t idC = -1;
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.95f}, {{0.8f, 0.8f}, 0.95f}};
    const auto& ids = trk.update(dets, dt);
    idC = ids[0];
    REQUIRE(idC == 3); // ids are monotonic: A's id is NOT recycled
    REQUIRE(find_track(trk, idC)->state == ossia::track_state::confirmed);
    REQUIRE(find_track(trk, idC)->slot == -1); // held back by the quarantine
  }

  // Once the hold lapses, C picks slot 0 up via the retry pass.
  bool got_slot = false;
  for(int f = 0; f < 30 && !got_slot; f++)
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.95f}, {{0.8f, 0.8f}, 0.95f}};
    trk.update(dets, dt);
    got_slot = find_track(trk, idC)->slot == 0;
  }
  REQUIRE(got_slot);
  REQUIRE(find_track(trk, idB)->slot == 1); // B never moved
}

TEST_CASE("provisional tracks die fast, confirmed ones coast", "[point_tracker]")
{
  T2 trk;
  trk.configure(base_cfg());
  const float dt = 1.f / 30.f;

  // Two hits, then silence: with M=3/N=5 the track may only miss 2 in a row.
  for(int f = 0; f < 2; f++)
  {
    std::vector<D2> dets{{{0.5f, 0.5f}, 0.8f}};
    trk.update(dets, dt);
  }
  REQUIRE(trk.tracks().size() == 1);
  REQUIRE(trk.tracks()[0].state == ossia::track_state::provisional);
  int exited = 0;
  for(int f = 0; f < 5; f++)
  {
    trk.update(nullptr, 0, dt);
    exited += trk.events().exited.size();
  }
  REQUIRE(trk.tracks().empty());
  REQUIRE(exited == 1); // paired with its entered event
}

// ---------------------------------------------------------------------------
// Process-level tests: avnd_tools::PointTracker2D driven through its tick.
// ---------------------------------------------------------------------------

namespace
{
struct ProcessHarness
{
  avnd_tools::PointTracker2D p;
  std::int64_t ns = 1'000'000'000; // arbitrary nonzero start
  std::vector<int> entered, confirmed, exited, revived;

  ProcessHarness()
  {
    p.prepare(halp::setup{.rate = 48000.});
    hook(p.outputs.entered, entered);
    hook(p.outputs.confirmed, confirmed);
    hook(p.outputs.exited, exited);
    hook(p.outputs.revived, revived);
  }

  template <typename CB>
  static void hook(CB& cb, std::vector<int>& sink)
  {
    cb.call.context = &sink;
    cb.call.function
        = [](void* ctx, int id) { static_cast<std::vector<int>*>(ctx)->push_back(id); };
  }

  // One tick of `ms` milliseconds, optionally delivering a detection frame.
  void tick(double ms, std::vector<ossia::value> pts = {}, bool with_points = false)
  {
    if(with_points)
    {
      p.inputs.points.value = std::move(pts);
      p.points_dirty = true;
    }
    halp::tick_musical tk;
    tk.frames = int(48.0 * ms);
    ns += std::int64_t(ms * 1e6);
    tk.position_in_nanoseconds = double(ns);
    p(tk);
  }
};
}

TEST_CASE("process: flat output packing, compact and slots", "[point_tracker_process]")
{
  ProcessHarness h;
  h.p.inputs.slot_count.value = 4;
  h.p.inputs.format.value = avnd_tools::TrackerFormat::Compact;
  h.p.inputs.deadband.value = 0.f;

  // Two detections as vec2f, full confidence => instant-confirmed.
  std::vector<ossia::value> pts{ossia::vec2f{0.25f, 0.5f}, ossia::vec2f{0.75f, 0.5f}};
  for(int f = 0; f < 3; f++)
    h.tick(33.0, pts, true);

  REQUIRE(h.p.outputs.count.value == 2);
  REQUIRE(h.entered.size() == 2);
  REQUIRE(h.confirmed.size() == 2);

  // Compact: [x0, y0, x1, y1] ordered by id.
  {
    const auto& pos = h.p.outputs.positions.value;
    const auto& ids = h.p.outputs.ids.value;
    REQUIRE(pos.size() == 4);
    REQUIRE(ids == std::vector<int>{1, 2});
    REQUIRE(pos[0] == Catch::Approx(0.25).margin(0.01));
    REQUIRE(pos[1] == Catch::Approx(0.5).margin(0.01));
    REQUIRE(pos[2] == Catch::Approx(0.75).margin(0.01));
    REQUIRE(pos[3] == Catch::Approx(0.5).margin(0.01));
  }

  // Slots: fixed stride, slot-indexed, zero-padded: slot i starts at i * N.
  h.p.inputs.format.value = avnd_tools::TrackerFormat::Slots;
  h.tick(33.0, pts, true);
  {
    const auto& pos = h.p.outputs.positions.value;
    const auto& ids = h.p.outputs.ids.value;
    REQUIRE(pos.size() == 8); // 4 slots * 2 floats
    REQUIRE(ids.size() == 4);
    REQUIRE(ids[0] == 1);
    REQUIRE(ids[1] == 2);
    REQUIRE(ids[2] == -1);
    REQUIRE(ids[3] == -1);
    REQUIRE(pos[0] == Catch::Approx(0.25).margin(0.01));
    REQUIRE(pos[2] == Catch::Approx(0.75).margin(0.01));
    REQUIRE(pos[4] == 0.f); // zero padding
    REQUIRE(pos[5] == 0.f);
    REQUIRE(pos[6] == 0.f);
    REQUIRE(pos[7] == 0.f);
  }

  // Records carry the metadata.
  {
    const auto& recs = h.p.outputs.tracks.value;
    REQUIRE(recs.size() == 2);
    REQUIRE(recs[0].id == 1);
    REQUIRE(recs[0].slot == 0);
    REQUIRE(recs[0].state == "confirmed");
    REQUIRE(!recs[0].provisional);
  }
}

TEST_CASE("process: input format flexibility", "[point_tracker_process]")
{
  // Flat number list [x, y, x, y]
  {
    ProcessHarness h;
    h.p.inputs.instant_confirm.value = 1.5f; // fully manual confirmation
    h.tick(33.0, {0.1f, 0.2f, 0.6f, 0.7f}, true);
    REQUIRE(h.p.outputs.count.value == 2); // provisional but emitted
    REQUIRE(h.p.outputs.tracks.value[0].provisional);
  }
  // Sub-lists with confidence
  {
    ProcessHarness h;
    std::vector<ossia::value> pts{
        std::vector<ossia::value>{0.3f, 0.4f, 0.95f},
    };
    h.tick(33.0, pts, true);
    REQUIRE(h.p.outputs.count.value == 1);
    REQUIRE(h.p.outputs.tracks.value[0].confidence == Catch::Approx(0.95));
  }
  // Maps with centroid + confidence (the Blob stats shape)
  {
    ProcessHarness h;
    ossia::value_map_type m;
    m.emplace_back("centroid", ossia::vec2f{0.5f, 0.5f});
    m.emplace_back("confidence", 0.8f);
    h.tick(33.0, {ossia::value{m}}, true);
    REQUIRE(h.p.outputs.count.value == 1);
    REQUIRE(h.p.outputs.tracks.value[0].position_raw.x == Catch::Approx(0.5));
  }
  // Emit Unconfirmed off: provisional tracks hidden
  {
    ProcessHarness h;
    h.p.inputs.emit_unconfirmed.value = false;
    h.tick(33.0, {ossia::vec2f{0.1f, 0.2f}}, true); // conf 1 -> instant confirm
    REQUIRE(h.p.outputs.count.value == 1);
    h.p.inputs.instant_confirm.value = 2.f; // now nothing can insta-confirm
    ProcessHarness h2;
    h2.p.inputs.emit_unconfirmed.value = false;
    h2.p.inputs.instant_confirm.value = 1.5f;
    std::vector<ossia::value> low{std::vector<ossia::value>{0.4f, 0.4f, 0.7f}};
    h2.tick(33.0, low, true);
    REQUIRE(h2.p.outputs.count.value == 0); // provisional, not emitted
    REQUIRE(h2.entered.size() == 1);        // but the event did fire
  }
}

TEST_CASE("process: exit events fire when the source goes silent", "[point_tracker_process]")
{
  ProcessHarness h;
  h.p.inputs.coast_time.value = 200.f; // ms
  h.p.inputs.revive.value = false;
  std::vector<ossia::value> pts{ossia::vec2f{0.5f, 0.5f}};
  for(int f = 0; f < 5; f++)
    h.tick(33.0, pts, true);
  REQUIRE(h.p.outputs.count.value == 1);

  // Source stops entirely: only empty ticks from here.
  for(int f = 0; f < 30; f++)
    h.tick(33.0);
  REQUIRE(h.exited.size() == 1);
  REQUIRE(h.p.outputs.count.value == 0);
  REQUIRE(h.p.outputs.positions.value.empty());
}

// ---------------------------------------------------------------------------
// Port type round-trips through ossia::value (the Bug-2 regression, applied to
// this process's own types).
// ---------------------------------------------------------------------------

TEST_CASE("track_record round-trips through ossia::value", "[point_tracker_process]")
{
  using R2 = avnd_tools::PointTracker2D::track_record;
  R2 r;
  r.id = 7;
  r.slot = 2;
  r.state = "confirmed";
  r.creation_time = 12.5;
  r.age = 3.25f;
  r.time_since_seen = 0.033f;
  r.position = {0.25f, 0.75f};
  r.position_raw = {0.26f, 0.74f};
  r.velocity = {-0.5f, 1.5f};
  r.confidence = 0.9f;
  r.provisional = false;
  r.reacquired = true;

  const ossia::value v = oscr::to_ossia_value(r);

  // The record is a map, and the positions inside are plain vec2f (xy_type has
  // no field names, deliberately - see the header).
  {
    const auto* map = v.target<ossia::value_map_type>();
    REQUIRE(map != nullptr);
    bool found = false;
    for(const auto& [k, val] : *map)
      if(k == "position")
      {
        REQUIRE(val.get_type() == ossia::val_type::VEC2F);
        found = true;
      }
    REQUIRE(found);
  }

  R2 out;
  REQUIRE(oscr::from_ossia_value(v, out));
  REQUIRE(out.id == 7);
  REQUIRE(out.slot == 2);
  REQUIRE(out.state == "confirmed");
  REQUIRE(out.creation_time == Catch::Approx(12.5));
  REQUIRE(out.age == Catch::Approx(3.25));
  REQUIRE(out.position.x == 0.25f);
  REQUIRE(out.position.y == 0.75f);
  REQUIRE(out.velocity.x == -0.5f);
  REQUIRE(out.velocity.y == 1.5f);
  REQUIRE(out.confidence == 0.9f);
  REQUIRE(out.provisional == false);
  REQUIRE(out.reacquired == true);

  // And as a list element, the common cabling shape.
  std::vector<R2> list{r, r};
  const ossia::value vl = oscr::to_ossia_value(list);
  std::vector<R2> out_list;
  REQUIRE(oscr::from_ossia_value(vl, out_list));
  REQUIRE(out_list.size() == 2);
  REQUIRE(out_list[1].position.y == 0.75f);
  REQUIRE(out_list[1].id == 7);
}

TEST_CASE("3D track_record round-trips with vec3f positions", "[point_tracker_process]")
{
  using R3 = avnd_tools::PointTracker3D::track_record;
  R3 r;
  r.id = 1;
  r.state = "provisional";
  r.position = {1.f, 2.f, 3.f};
  r.velocity = {0.1f, 0.2f, 0.3f};

  const ossia::value v = oscr::to_ossia_value(r);
  R3 out;
  REQUIRE(oscr::from_ossia_value(v, out));
  REQUIRE(out.position.x == 1.f);
  REQUIRE(out.position.y == 2.f);
  REQUIRE(out.position.z == 3.f);
  REQUIRE(out.velocity.z == Catch::Approx(0.3f));
}
