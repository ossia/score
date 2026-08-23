#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/detail/flat_map.hpp>
#include <ossia/math/point_tracker.hpp>
#include <ossia/network/value/value.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.enums.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/value_types.hpp>

#include <cmath>

#include <string>
#include <vector>

namespace avnd_tools
{

// Point Tracker — the association + filtering + lifecycle layer that turns
// per-frame sets of bare detections (points, optionally with a confidence)
// into stable identified tracks over time. It is NOT a detector: feed it
// whatever produces points — Blob stats centroids, pose keypoints, TUIO/OSC
// cursors, custom scripts — and map the resulting ids to audio and visuals.
//
// Coordinate space: whatever the upstream source uses, consistently. The
// computer-vision objects in the tree normalize to [0,1] on x and y; nothing
// anywhere normalizes z, so in 3D use whatever unit your source speaks and
// scale Max Speed & friends accordingly (all speed/noise knobs are in the
// input's coordinate units).
//
// Input format ("Points"): a list, each element being one detection —
//   * a vec2f / vec3f (vec3f in 2D = x, y, confidence;
//     in 3D a vec4f = x, y, z, confidence),
//   * a sub-list of numbers [x, y, (z), (confidence)],
//   * a map with keys position/pos/centroid/point (+ confidence/score/conf),
//     which is what Blob stats / Blob sort emit,
//   * or a flat list of plain numbers [x, y, x, y, ...] (stride 2 in 2D, 3 in
//     3D) for the whole frame at once.
//
// Two-tier output, the key latency decision: a track confirmed over 3 frames
// at 30 fps costs 100 ms of onset latency — an order of magnitude over the
// ~10 ms budget of musical control. So provisional tracks are emitted
// immediately (flagged, for triggers) while confirmed tracks form the stable
// set for continuous mappings. Turn "Emit Unconfirmed" off to opt out.
//
// Identity: `id` is persistent and NEVER reused; `creation_time` is the true
// identity if ids are ever reset. `slot` is a dense reusable index in
// [0, Slot Count[ for mapping tracks to a fixed bank of voices/parameters,
// recycled through a quarantine hold so a brief exit/re-entry does not hand an
// object's slot to a stranger.

enum class TrackerMotionGate
{
  MaxSpeed,
  Mahalanobis,
  Off
};
enum class TrackerAllocation
{
  LowestFree,
  RoundRobin,
  NearestVacated
};
enum class TrackerSteal
{
  Never,
  Stalest,
  LowestConfidence
};
enum class TrackerOrder
{
  Id,
  Slot,
  Age,
  Confidence,
  DistanceToAnchor
};
enum class TrackerFormat
{
  Compact,
  Slots
};

template <std::size_t N>
struct PointTrackerBase
{
  static_assert(N == 2 || N == 3);
  using position_type
      = std::conditional_t<N == 2, halp::xy_type<float>, halp::xyz_type<float>>;
  using tracker_type = ossia::point_tracker<N>;
  using detection_type = ossia::point_detection<N>;

  // One emitted track. Serialized as a map (field names); the positions inside
  // use halp::xy_type / halp::xyz_type on purpose: they carry no field names
  // and therefore encode as plain vec2f / vec3f in both directions.
  struct track_record
  {
    int id = -1;
    int slot = -1;
    std::string state; // provisional | confirmed | coasting | revived
    double creation_time = 0.;
    float age = 0.f;
    float time_since_seen = 0.f;
    position_type position{};     // smoothed (One-Euro), lead-compensated
    position_type position_raw{}; // last raw measurement
    position_type velocity{};     // units per second
    float confidence = 0.f;
    bool provisional = false;
    bool reacquired = false;

    halp_field_names(
        id, slot, state, creation_time, age, time_since_seen, position,
        position_raw, velocity, confidence, provisional, reacquired);
  };

  struct ins
  {
    struct : halp::val_port<"Points", std::vector<ossia::value>>
    {
      // Dimension-specific: advertising a z coordinate on the 2D process sends
      // people looking for a control that is not there.
      halp_meta(
          description,
          N == 2
              ? "Detections for this frame. Accepts a list of vec2f, a list of "
                "vec3f read as (x, y, confidence), sub-lists [x, y] or "
                "[x, y, confidence], {position, confidence} maps as emitted by "
                "Blob stats, or one flat list of numbers [x, y, x, y, ...]."
              : "Detections for this frame. Accepts a list of vec3f, a list of "
                "vec4f read as (x, y, z, confidence), sub-lists [x, y, z] or "
                "[x, y, z, confidence], {position, confidence} maps, or one "
                "flat list of numbers [x, y, z, x, y, z, ...].")
      void update(auto& self) { self.points_dirty = true; }
    } points;

    // --- Association ---
    struct : halp::spinbox_f32<"Max Speed", halp::range{0.01, 100., 2.}>
    {
      halp_meta(
          description,
          "Fastest plausible object speed, in coordinate units per second - the "
          "primary association gate. 2 m/s suits human-scale motion in metric "
          "spaces; in [0,1] camera space 2.0 means crossing the frame in 0.5 s.")
    } max_speed;
    // Comboboxes rather than enum_t throughout: the enum ("choices") widget
    // lays every option out side by side, so a handful of multi-word options
    // spans the whole process width and wraps. A dropdown costs one line.
    struct : halp::combobox_t<"Motion Gate", TrackerMotionGate>
    {
      halp_meta(
          description,
          "How to reject implausible jumps: Max Speed (analytic budget "
          "max_speed*dt*(1 + lost/coast)), Mahalanobis (chi-square on the "
          "Kalman innovation), or Off.")
      struct range
      {
        std::string_view values[3]{"Max speed", "Mahalanobis", "Off"};
        TrackerMotionGate init{TrackerMotionGate::MaxSpeed};
      };
    } motion_gate;
    struct : halp::toggle<"Two-Stage Association", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "ByteTrack: after matching high-confidence detections, let "
          "low-confidence ones sustain still-unmatched recent tracks (they "
          "never birth or revive). Worth 1-10 IDF1 points on crowded scenes.")
    } two_stage;
    struct : halp::knob_f32<"High Confidence", halp::range{0., 1., 0.5}>
    {
      halp_meta(
          description,
          "Detections at or above this confidence are matched first and can "
          "sustain any track. Set it around the confidence your detector gives "
          "clearly-visible objects.")
    } high_conf;
    struct : halp::knob_f32<"Low Confidence", halp::range{0., 1., 0.1}>
    {
      halp_meta(
          description,
          "Detections between this and High Confidence only run in the "
          "second-stage recovery: they can sustain an existing track through an "
          "occlusion dip but never start or revive one. Below this they are "
          "dropped.")
    } low_conf;
    struct : halp::knob_f32<"New Track Confidence", halp::range{0., 1., 0.6}>
    {
      halp_meta(description, "Minimum confidence for a detection to start a new track.")
    } new_conf;
    struct : halp::knob_f32<"Direction Consistency", halp::range{0., 1., 0.2}>
    {
      halp_meta(
          description,
          "OC-SORT: penalize associations that reverse the track's direction of "
          "motion, averaged over 1-3 frame baselines so one noisy frame does "
          "not fake a turn. Helps crossings; 0 = off.")
    } dir_weight;
    struct : halp::knob_f32<"Confidence Modeling", halp::range{0., 1.5, 1.}>
    {
      halp_meta(
          description,
          "Hybrid-SORT: prefer the detection whose confidence continues the "
          "track's confidence trend (an occluded object's confidence sinks "
          "smoothly, so the trend tells crossing objects apart when position "
          "cannot). Needs a detector with meaningful per-detection confidence; "
          "0 = off.")
    } conf_weight;

    // --- Motion model ---
    struct : halp::spinbox_f32<"Motion Noise", halp::range{0.001, 50., 0.67}>
    {
      halp_meta(
          description,
          "Kalman process noise: expected acceleration (units/s^2). Human "
          "motion peaks near 2 m/s^2; 0.67 fits that as 3 sigma. Raise for "
          "erratic motion, lower for very smooth coasting.")
    } accel_sigma;
    struct : halp::spinbox_f32<"Position Noise", halp::range{0.0001, 1., 0.005}>
    {
      halp_meta(
          description,
          N == 2
              ? "Detector jitter (standard deviation, coordinate units). Match it "
                "to your detector's actual noise: the association gates widen by "
                "this amount. Understating it makes a track reject its own noisy "
                "detections and be reborn under a new id; overstating it widens "
                "the gates until neighbouring entities compete for the same "
                "detection and swap."
              : "Detector jitter (standard deviation, coordinate units). Match it "
                "to your detector's actual noise: the association gates widen by "
                "this amount. Understating it makes a track reject its own noisy "
                "detections and be reborn under a new id; overstating it widens "
                "the gates until neighbouring entities compete for the same "
                "detection and swap. This matters more in 3D than in 2D: noise "
                "on three axes gives a radial error of sigma*sqrt(3) against a "
                "gate that does not grow with the dimension.")
    } meas_noise;

    // --- Confirmation ---
    struct : halp::spinbox_f32<"Confirm Time", halp::range{0., 2000., 100.}>
    {
      halp_meta(
          description,
          "Minimum age (milliseconds, not frames) before a track can be "
          "confirmed - stable across frame rates.")
    } confirm_time;
    struct : halp::spinbox_i32<"Confirm Hits", halp::range{1, 31, 3}>
    {
      halp_meta(
          description,
          "A track is confirmed once it was detected in this many of the last "
          "Confirm Window detection frames (and is older than Confirm Time). "
          "Raise to demand more evidence before a track joins the stable set.")
    } confirm_hits;
    struct : halp::spinbox_i32<"Confirm Window", halp::range{1, 31, 5}>
    {
      halp_meta(
          description,
          "Size of the recent-frames window that Confirm Hits is counted over. "
          "A wider window tolerates more dropped frames on the way to "
          "confirmation.")
    } confirm_window;
    struct : halp::knob_f32<"Instant Confirm Above", halp::range{0., 1.001, 0.9}>
    {
      halp_meta(
          description,
          "A detection at or above this confidence confirms its track "
          "immediately, skipping M-of-N. Set above 1 to disable.")
    } instant_confirm;
    struct : halp::toggle<"Emit Unconfirmed", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Emit provisional tracks immediately (flagged) instead of sitting out "
          "the ~100 ms confirmation latency. For musical onsets, keep this on "
          "and filter on the `provisional` field where it matters.")
    } emit_unconfirmed;

    // --- Lifecycle ---
    struct : halp::spinbox_f32<"Coast Time", halp::range{0., 5000., 500.}>
    {
      halp_meta(
          description,
          "After a miss, keep emitting the Kalman-predicted position for this "
          "long (ms) before the track goes lost.")
    } coast_time;
    struct : halp::toggle<"Revive", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Keep lost tracks in memory after coasting ends, so an object that "
          "reappears nearby gets its old id back instead of a new one.")
    } revive;
    struct : halp::spinbox_f32<"Revive Time", halp::range{0., 10000., 2000.}>
    {
      halp_meta(
          description,
          "After coasting ends, keep the lost track (unemitted) for this long "
          "(ms); a matching detection revives it with its id intact.")
    } revive_time;
    struct : halp::toggle<"Re-update on Revive", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "OC-SORT ORU: on revival, re-run the filter along a virtual "
          "trajectory across the gap, killing the post-occlusion lurch.")
    } revive_reupdate;

    // --- Output smoothing ---
    struct : halp::toggle<"Smooth", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "One-Euro filter on the output positions (per track, per axis): "
          "removes detector jitter at rest with minimal lag during motion. "
          "Tune with Min Cutoff and Beta.")
    } smooth;
    struct : halp::log_hslider_f32<"Min Cutoff", halp::range{0.005, 10., 1.}>
    {
      halp_meta(
          description, "One-Euro cutoff at rest, Hz. Lower = smoother = laggier.")
    } min_cutoff;
    struct : halp::hslider_f32<"Beta", halp::range{0., 100., 1.}>
    {
      halp_meta(
          description,
          "One-Euro speed coefficient: how much motion raises the cutoff. "
          "MediaPipe ships 10-80 on landmarks; raise it if fast motion lags.")
    } beta;
    struct : halp::spinbox_f32<"Prediction Lead", halp::range{0., 100., 0.}>
    {
      halp_meta(
          description,
          "Output position = estimate + velocity * lead (ms): compensates "
          "downstream latency at the price of overshoot.")
    } lead;
    struct : halp::spinbox_f32<"Output Deadband", halp::range{0., 0.1, 0.0005}>
    {
      halp_meta(
          description,
          "Suppress output changes smaller than this distance (coordinate "
          "units; ~half a pixel in normalized camera space). 0 = off.")
    } deadband;

    // --- Slots ---
    struct : halp::spinbox_i32<"Slot Count", halp::range{0, 64, 8}>
    {
      halp_meta(
          description,
          "Size of the dense reusable slot bank (for mapping to a fixed set of "
          "voices/parameters). 0 disables slots.")
    } slot_count;
    struct : halp::combobox_t<"Allocation", TrackerAllocation>
    {
      halp_meta(
          description,
          "How a newly confirmed track picks a slot: Lowest Free (deterministic, "
          "cv.jit-style), Round Robin (spread reuse over the bank, so a dead "
          "voice gets time to fade), or Nearest Vacated (an object re-entering "
          "where one just left inherits that slot).")
      struct range
      {
        std::string_view values[3]{"Lowest free", "Round robin", "Nearest vacated"};
        TrackerAllocation init{TrackerAllocation::LowestFree};
      };
    } allocation;
    struct : halp::combobox_t<"Steal Policy", TrackerSteal>
    {
      halp_meta(
          description,
          "When every slot is taken and a new track confirms: Never (it stays "
          "unslotted until one frees up), or steal from the Stalest (unseen the "
          "longest) or Lowest Confidence track.")
      struct range
      {
        std::string_view values[3]{"Never", "Stalest", "Lowest confidence"};
        TrackerSteal init{TrackerSteal::Never};
      };
    } steal;
    struct : halp::spinbox_f32<"Hold Time", halp::range{0., 5000., 250.}>
    {
      halp_meta(
          description,
          "Quarantine (ms): a vacated slot is not handed to a new track for "
          "this long, so a brief dropout does not shuffle the whole bank.")
    } hold_time;

    // --- Output ---
    struct : halp::combobox_t<"Order By", TrackerOrder>
    {
      halp_meta(description, "Ordering of the compact outputs.")
      struct range
      {
        std::string_view values[5]{"Id", "Slot", "Age", "Confidence", "Distance to anchor"};
        TrackerOrder init{TrackerOrder::Id};
      };
    } order_by;
    struct
        : std::conditional_t<
              N == 2, halp::xy_spinboxes_f32<"Anchor", halp::range{-1000., 1000., 0.}>,
              halp::xyz_spinboxes_f32<"Anchor", halp::range{-1000., 1000., 0.}>>
    {
      halp_meta(
          description, "Reference point for the Distance To Anchor ordering.")
    } anchor;
    struct : halp::combobox_t<"Data Format", TrackerFormat>
    {
      halp_meta(
          description,
          "Positions/Ids layout: Compact = live tracks back-to-back in Order By "
          "order; Slots = Slot Count fixed positions indexed by slot, "
          "zero-padded (GPU/voice-bank friendly).")
      struct range
      {
        std::string_view values[2]{"Compact", "Slots"};
        TrackerFormat init{TrackerFormat::Compact};
      };
    } format;

    struct : halp::impulse_button<"Reset IDs">
    {
      halp_meta(description, "Forget every track and restart ids from 1.")
      void update(auto& self) { self.reset_requested = true; }
    } reset_ids;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Tracks")
      halp_meta(
          description,
          "Every emitted track as a record: id, slot, state, creation_time, "
          "age, time_since_seen, position, position_raw, velocity, confidence, "
          "provisional, reacquired.")
      std::vector<track_record> value;
    } tracks;

    struct
    {
      halp_meta(name, "Count")
      halp_meta(
          description,
          "Number of currently emitted tracks (confirmed + coasting + revived, "
          "plus provisional ones when Emit Unconfirmed is on).")
      int value{};
    } count;

    struct
    {
      halp_meta(name, "Positions")
      halp_meta(
          description,
          "Flat [x,y,...] float list of the smoothed track positions, in the "
          "chosen Data Format. Cables directly into Point2D View, Array to "
          "Mesh, and other renderers.")
      std::vector<float> value;
    } positions;

    struct
    {
      halp_meta(name, "Ids")
      halp_meta(
          description,
          "Track id per entry of Positions (-1 for an empty slot in Slots "
          "format).")
      std::vector<int> value;
    } ids;

    struct : halp::callback<"Entered", int>
    {
      halp_meta(
          description,
          "Fires with the track id the instant a detection births a new track "
          "(before confirmation) - the lowest-latency onset signal.")
    } entered;
    struct : halp::callback<"Confirmed", int>
    {
      halp_meta(
          description,
          "Fires with the track id when a track passes confirmation and joins "
          "the stable set.")
    } confirmed;
    struct : halp::callback<"Exited", int>
    {
      halp_meta(
          description,
          "Fires with the track id when a track is removed for good (coast and "
          "revival windows exhausted). Use it to release voices/mappings.")
    } exited;
    struct : halp::callback<"Revived", int>
    {
      halp_meta(
          description,
          "Fires with the track id when a lost track is re-acquired with its "
          "identity intact.")
    } revived;
  } outputs;

  // Grouped by the question being answered rather than by pipeline order:
  // "which detection belongs to which track", "when does a track begin and
  // end", "how steady is the output", "how do tracks map onto a fixed bank",
  // "what comes out". Every control appears in exactly one tab; the Points
  // inlet is a cable, not a control, so it has no widget here.
  struct ui
  {
    halp_meta(name, "Point Tracker")
    halp_meta(layout, halp::layouts::tabs)
    halp_meta(background, halp::colors::background_mid)

    struct
    {
      halp_meta(name, "Association")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Gating")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::motion_gate> motion_gate;
        halp::item<&ins::max_speed> max_speed;
        halp::item<&ins::accel_sigma> accel_sigma;
        halp::item<&ins::meas_noise> meas_noise;
      } gating;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Confidence")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::two_stage> two_stage;
        halp::item<&ins::high_conf> high_conf;
        halp::item<&ins::low_conf> low_conf;
        halp::item<&ins::new_conf> new_conf;
      } confidence;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Extra cues")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::dir_weight> dir_weight;
        halp::item<&ins::conf_weight> conf_weight;
      } cues;
    } association_tab;

    struct
    {
      halp_meta(name, "Lifecycle")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Confirmation")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::confirm_time> confirm_time;
        halp::item<&ins::confirm_hits> confirm_hits;
        halp::item<&ins::confirm_window> confirm_window;
        halp::item<&ins::instant_confirm> instant_confirm;
        halp::item<&ins::emit_unconfirmed> emit_unconfirmed;
      } confirmation;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Coast & revive")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::coast_time> coast_time;
        halp::item<&ins::revive> revive;
        halp::item<&ins::revive_time> revive_time;
        halp::item<&ins::revive_reupdate> revive_reupdate;
      } coasting;
    } lifecycle_tab;

    struct
    {
      halp_meta(name, "Smoothing")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Filter")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::smooth> smooth;
        halp::item<&ins::min_cutoff> min_cutoff;
        halp::item<&ins::beta> beta;
      } filter;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Latency")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::lead> lead;
        halp::item<&ins::deadband> deadband;
      } latency;
    } smoothing_tab;

    struct
    {
      halp_meta(name, "Slots & output")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Bank")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::slot_count> slot_count;
        halp::item<&ins::allocation> allocation;
      } bank;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Reuse")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::steal> steal;
        halp::item<&ins::hold_time> hold_time;
        halp::item<&ins::reset_ids> reset_ids;
      } reuse;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Output")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::format> format;
        halp::item<&ins::order_by> order_by;
        halp::item<&ins::anchor> anchor;
      } output;
    } slots_tab;
  };

  using tick = halp::tick_musical;

  void prepare(halp::setup s)
  {
    if(s.rate > 0)
      m_rate = s.rate;
  }

  void operator()(const halp::tick_musical& tk)
  {
    // Real elapsed time for this tick.
    double dt = 0.;
    if(tk.position_in_nanoseconds > 0 && m_last_ns > 0
       && tk.position_in_nanoseconds > m_last_ns)
      dt = (tk.position_in_nanoseconds - m_last_ns) * 1e-9;
    else if(tk.frames > 0)
      dt = tk.frames / m_rate;
    if(tk.position_in_nanoseconds > 0)
      m_last_ns = tk.position_in_nanoseconds;
    dt = std::clamp(dt, 0., 2.);

    if(reset_requested)
    {
      reset_requested = false;
      m_tracker.reset();
      m_last_out.clear();
    }

    apply_config();

    bool recompute = false;
    if(points_dirty)
    {
      points_dirty = false;
      parse_detections();
      m_tracker.update(m_dets.data(), m_dets.size(), float(dt));
      recompute = true;
    }
    else
    {
      m_tracker.advance(float(dt));
      // While the source is silent, re-emit coasting predictions at roughly
      // the source's own cadence so downstream keeps moving smoothly.
      m_silent_acc += dt;
      if(m_silent_acc >= m_tracker.estimated_period() && has_emitted_tracks())
        recompute = true;
    }

    const auto& ev = m_tracker.events();
    if(!ev.entered.empty() || !ev.confirmed.empty() || !ev.exited.empty()
       || !ev.revived.empty())
      recompute = true;

    for(auto id : ev.entered)
      outputs.entered(id);
    for(auto id : ev.confirmed)
      outputs.confirmed(id);
    for(auto id : ev.revived)
      outputs.revived(id);
    for(auto id : ev.exited)
    {
      m_last_out.erase(id);
      outputs.exited(id);
    }

    if(recompute)
    {
      m_silent_acc = 0.;
      emit_outputs();
    }
  }

  bool points_dirty = false;
  bool reset_requested = false;

private:
  using track_t = typename tracker_type::track;

  void apply_config()
  {
    auto& c = m_cfg;
    c.max_speed = std::max(inputs.max_speed.value, 1e-4f);
    switch(inputs.motion_gate.value)
    {
      case TrackerMotionGate::MaxSpeed:
        c.gate = ossia::track_motion_gate::max_speed;
        break;
      case TrackerMotionGate::Mahalanobis:
        c.gate = ossia::track_motion_gate::mahalanobis;
        break;
      case TrackerMotionGate::Off:
        c.gate = ossia::track_motion_gate::off;
        break;
    }
    c.two_stage = inputs.two_stage.value;
    c.high_conf = inputs.high_conf.value;
    c.low_conf = inputs.low_conf.value;
    c.new_conf = inputs.new_conf.value;
    c.dir_weight = inputs.dir_weight.value;
    c.conf_weight = inputs.conf_weight.value;
    c.accel_sigma = inputs.accel_sigma.value;
    c.meas_std = inputs.meas_noise.value;
    c.confirm_time = inputs.confirm_time.value * 1e-3f;
    c.confirm_hits = std::uint32_t(std::max(inputs.confirm_hits.value, 1));
    c.confirm_window = std::uint32_t(std::clamp(inputs.confirm_window.value, 1, 31));
    c.instant_confirm = inputs.instant_confirm.value;
    c.coast_time = inputs.coast_time.value * 1e-3f;
    c.revive = inputs.revive.value;
    c.revive_time = inputs.revive_time.value * 1e-3f;
    c.revive_reupdate = inputs.revive_reupdate.value;
    c.smooth = inputs.smooth.value;
    c.min_cutoff = std::max(inputs.min_cutoff.value, 0.001f);
    c.beta = inputs.beta.value;
    c.slot_count = std::uint32_t(std::max(inputs.slot_count.value, 0));
    switch(inputs.allocation.value)
    {
      case TrackerAllocation::LowestFree:
        c.allocation = ossia::track_slot_allocation::lowest_free;
        break;
      case TrackerAllocation::RoundRobin:
        c.allocation = ossia::track_slot_allocation::round_robin;
        break;
      case TrackerAllocation::NearestVacated:
        c.allocation = ossia::track_slot_allocation::nearest_vacated;
        break;
    }
    switch(inputs.steal.value)
    {
      case TrackerSteal::Never:
        c.steal = ossia::track_slot_steal::never;
        break;
      case TrackerSteal::Stalest:
        c.steal = ossia::track_slot_steal::stalest;
        break;
      case TrackerSteal::LowestConfidence:
        c.steal = ossia::track_slot_steal::lowest_confidence;
        break;
    }
    c.slot_hold_time = inputs.hold_time.value * 1e-3f;
    m_tracker.configure(c);
  }

  static bool number_like(const ossia::value& v) noexcept
  {
    const auto t = v.get_type();
    return t == ossia::val_type::FLOAT || t == ossia::val_type::INT
           || t == ossia::val_type::BOOL;
  }

  static float to_float(const ossia::value& v) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::FLOAT:
        return *v.target<float>();
      case ossia::val_type::INT:
        return float(*v.target<int>());
      case ossia::val_type::BOOL:
        return *v.target<bool>() ? 1.f : 0.f;
      default:
        return 0.f;
    }
  }

  // One element of the input list -> one detection. Returns false if the
  // element is not something point-like.
  bool parse_element(const ossia::value& v, detection_type& out) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::VEC2F: {
        const auto& a = *v.target<ossia::vec2f>();
        if constexpr(N == 2)
        {
          out.position = {a[0], a[1]};
          out.confidence = 1.f;
          return true;
        }
        return false; // a 2D point has no meaning as a 3D detection
      }
      case ossia::val_type::VEC3F: {
        const auto& a = *v.target<ossia::vec3f>();
        if constexpr(N == 2)
        {
          out.position = {a[0], a[1]};
          out.confidence = a[2]; // x, y, confidence
        }
        else
        {
          out.position = {a[0], a[1], a[2]};
          out.confidence = 1.f;
        }
        return true;
      }
      case ossia::val_type::VEC4F: {
        const auto& a = *v.target<ossia::vec4f>();
        if constexpr(N == 2)
        {
          out.position = {a[0], a[1]};
          out.confidence = a[2];
        }
        else
        {
          out.position = {a[0], a[1], a[2]};
          out.confidence = a[3]; // x, y, z, confidence
        }
        return true;
      }
      case ossia::val_type::LIST: {
        const auto& l = *v.target<std::vector<ossia::value>>();
        if(l.size() < N)
          return false;
        for(std::size_t i = 0; i < N; i++)
        {
          if(!number_like(l[i]))
            return false;
          out.position[i] = to_float(l[i]);
        }
        out.confidence = (l.size() > N && number_like(l[N])) ? to_float(l[N]) : 1.f;
        return true;
      }
      case ossia::val_type::MAP: {
        const auto& m = *v.target<ossia::value_map_type>();
        bool has_pos = false;
        out.confidence = 1.f;
        for(const auto& [k, val] : m)
        {
          if(k == "position" || k == "pos" || k == "centroid" || k == "point")
          {
            detection_type sub;
            if(parse_element(val, sub))
            {
              out.position = sub.position;
              has_pos = true;
            }
          }
          else if(k == "confidence" || k == "score" || k == "conf")
          {
            if(number_like(val))
              out.confidence = to_float(val);
          }
        }
        return has_pos;
      }
      default:
        return false;
    }
  }

  void parse_detections()
  {
    m_dets.clear();
    const auto& in = inputs.points.value;
    if(in.empty())
      return;

    // A flat frame of plain numbers: [x, y, (z), x, y, (z), ...]
    if(number_like(in[0]))
    {
      for(std::size_t i = 0; i + N <= in.size(); i += N)
      {
        detection_type d;
        bool ok = true;
        for(std::size_t k = 0; k < N; k++)
        {
          if(!number_like(in[i + k]))
          {
            ok = false;
            break;
          }
          d.position[k] = to_float(in[i + k]);
        }
        if(!ok)
          break;
        d.confidence = 1.f;
        if(std::isfinite(d.position[0]))
          m_dets.push_back(d);
        if(m_dets.size() >= 1024)
          break;
      }
      return;
    }

    for(const auto& v : in)
    {
      detection_type d;
      if(parse_element(v, d) && std::isfinite(d.position[0])
         && std::isfinite(d.position[1]))
        m_dets.push_back(d);
      if(m_dets.size() >= 1024)
        break;
    }
  }

  bool has_emitted_tracks() const noexcept
  {
    for(const auto& t : m_tracker.tracks())
      if(t.emitted(inputs.emit_unconfirmed.value))
        return true;
    return false;
  }

  static const char* state_name(ossia::track_state s) noexcept
  {
    switch(s)
    {
      case ossia::track_state::provisional:
        return "provisional";
      case ossia::track_state::confirmed:
        return "confirmed";
      case ossia::track_state::coasting:
        return "coasting";
      case ossia::track_state::revived:
        return "revived";
      case ossia::track_state::lost:
        return "lost";
      case ossia::track_state::expired:
        return "expired";
    }
    return "?";
  }

  // Smoothed position + prediction lead + per-track deadband.
  std::array<float, N> output_position(const track_t& t)
  {
    std::array<float, N> p = t.filtered;
    const float lead_s = inputs.lead.value * 1e-3f;
    if(lead_s > 0.f)
    {
      const auto v = t.velocity();
      for(std::size_t i = 0; i < N; i++)
        p[i] += v[i] * lead_s;
    }

    const float db = inputs.deadband.value;
    if(db > 0.f)
    {
      auto it = m_last_out.find(t.id);
      if(it != m_last_out.end())
      {
        float d2 = 0.f;
        for(std::size_t i = 0; i < N; i++)
        {
          const float d = p[i] - it->second[i];
          d2 += d * d;
        }
        if(d2 < db * db)
          return it->second; // hold the previous output
        it->second = p;
      }
      else
      {
        m_last_out.emplace(t.id, p);
      }
    }
    return p;
  }

  static position_type to_position(const std::array<float, N>& a) noexcept
  {
    if constexpr(N == 2)
      return {a[0], a[1]};
    else
      return {a[0], a[1], a[2]};
  }

  void emit_outputs()
  {
    // Collect emitted tracks
    m_order.clear();
    for(const auto& t : m_tracker.tracks())
      if(t.emitted(inputs.emit_unconfirmed.value))
        m_order.push_back(&t);

    // Order the compact view
    const auto key_less = [this](const track_t* a, const track_t* b) {
      switch(inputs.order_by.value)
      {
        case TrackerOrder::Id:
          return a->id < b->id;
        case TrackerOrder::Slot: {
          // Unslotted tracks last, then by id for determinism
          const auto sa = a->slot < 0 ? INT32_MAX : a->slot;
          const auto sb = b->slot < 0 ? INT32_MAX : b->slot;
          return sa != sb ? sa < sb : a->id < b->id;
        }
        case TrackerOrder::Age:
          return a->age != b->age ? a->age > b->age : a->id < b->id;
        case TrackerOrder::Confidence:
          return a->confidence != b->confidence ? a->confidence > b->confidence
                                                : a->id < b->id;
        case TrackerOrder::DistanceToAnchor: {
          const float da = anchor_distance2(*a), db_ = anchor_distance2(*b);
          return da != db_ ? da < db_ : a->id < b->id;
        }
      }
      return a->id < b->id;
    };
    std::sort(m_order.begin(), m_order.end(), key_less);

    // Records
    auto& recs = outputs.tracks.value;
    recs.clear();
    recs.reserve(m_order.size());
    for(const track_t* t : m_order)
    {
      track_record r;
      r.id = t->id;
      r.slot = t->slot;
      r.state = state_name(t->state);
      r.creation_time = t->creation_time;
      r.age = t->age;
      r.time_since_seen = t->time_since_seen;
      r.position = to_position(output_position(*t));
      r.position_raw = to_position(t->last_meas);
      r.velocity = to_position(t->velocity());
      r.confidence = t->confidence;
      r.provisional = t->state == ossia::track_state::provisional;
      r.reacquired = t->reacquired;
      recs.push_back(std::move(r));
    }
    outputs.count.value = int(m_order.size());

    // Flat positions + ids
    auto& pos = outputs.positions.value;
    auto& ids = outputs.ids.value;
    pos.clear();
    ids.clear();
    if(inputs.format.value == TrackerFormat::Slots)
    {
      const int slots = std::max(inputs.slot_count.value, 0);
      pos.assign(std::size_t(slots) * N, 0.f);
      ids.assign(std::size_t(slots), -1);
      for(const track_t* t : m_order)
      {
        if(t->slot < 0 || t->slot >= slots)
          continue;
        const auto p = output_position(*t);
        for(std::size_t i = 0; i < N; i++)
          pos[std::size_t(t->slot) * N + i] = p[i];
        ids[std::size_t(t->slot)] = t->id;
      }
    }
    else
    {
      pos.reserve(m_order.size() * N);
      ids.reserve(m_order.size());
      for(const track_t* t : m_order)
      {
        const auto p = output_position(*t);
        for(std::size_t i = 0; i < N; i++)
          pos.push_back(p[i]);
        ids.push_back(t->id);
      }
    }
  }

  float anchor_distance2(const track_t& t) const noexcept
  {
    const auto& a = inputs.anchor.value;
    const auto p = t.filtered;
    float d2 = (p[0] - a.x) * (p[0] - a.x) + (p[1] - a.y) * (p[1] - a.y);
    if constexpr(N == 3)
      d2 += (p[2] - a.z) * (p[2] - a.z);
    return d2;
  }

private:
  tracker_type m_tracker;
  typename tracker_type::config m_cfg;
  std::vector<detection_type> m_dets;
  std::vector<const track_t*> m_order;
  ossia::flat_map<std::int32_t, std::array<float, N>> m_last_out;
  double m_rate = 48000.;
  std::int64_t m_last_ns = 0;
  double m_silent_acc = 0.;
};

struct PointTracker2D : PointTrackerBase<2>
{
  halp_meta(name, "Point Tracker 2D")
  halp_meta(c_name, "avnd_point_tracker_2d")
  halp_meta(category, "Spatial/Tracking")
  halp_meta(author, "ossia team")
  halp_meta(
      description,
      "Turn flickering 2D detections (blobs, keypoints, TUIO cursors) into "
      "stable identified tracks: association, Kalman + One-Euro filtering, "
      "confirm/coast/revive lifecycle, persistent ids and voice-bank slots.")
  halp_meta(uuid, "2ef5f23f-2d22-4163-b299-976c9d9bd1c8")
};

struct PointTracker3D : PointTrackerBase<3>
{
  halp_meta(name, "Point Tracker 3D")
  halp_meta(c_name, "avnd_point_tracker_3d")
  halp_meta(category, "Spatial/Tracking")
  halp_meta(author, "ossia team")
  halp_meta(
      description,
      "Turn flickering 3D detections into stable identified tracks: "
      "association, Kalman + One-Euro filtering, confirm/coast/revive "
      "lifecycle, persistent ids and voice-bank slots.")
  halp_meta(uuid, "fa258031-25db-462f-8cc6-b3f049c8f6a7")
};

}
