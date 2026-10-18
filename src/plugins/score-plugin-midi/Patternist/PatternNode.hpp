#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <Patternist/PatternModel.hpp>
#include <libremidi/ump_events.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <random>

namespace Patternist
{
// The model stores channels as 1-16, the wire format as 0-15
inline uint8_t to_midi_channel(int c) noexcept
{
  return std::clamp(c - 1, 0, 15);
}

class pattern_node : public ossia::nonowning_graph_node
{
public:
  ossia::value_inlet pattern_select;
  ossia::value_inlet switch_quantification;
  ossia::value_inlet fill_in;
  ossia::value_inlet density_in;
  ossia::value_inlet velocity_in;
  ossia::value_inlet swing_in;
  ossia::value_inlet ratchet_in;
  ossia::value_inlet pace_in;
  ossia::value_inlet repeat_time_in;
  ossia::midi_outlet out;
  ossia::value_outlet accent_out;
  ossia::value_outlet slide_out;

  // The whole list, so that switching pattern moves an index rather than
  // copying lanes - which would allocate, on the audio thread.
  std::vector<Pattern> patterns;
  //! For each note, how many of the lanes' notes are sounding on it: two
  //! lanes on one note share a single voice, released by the last of them.
  std::array<uint8_t, 128> in_flight{};

  int current_pattern{};
  //! Pattern the next quantification point switches to; -1 when nothing waits.
  int pending_pattern{-1};
  //! A token_request rate: 0 switches at the beginning of the tick.
  double switch_rate{1.};

  bool fill{};
  float density{1.f};
  float velocity_scale{1.f};
  float swing{0.f};
  //! Multiplies every step's ratchet, 1 - 8, fractions included.
  float ratchet_scale{1.f};
  //! Added to every step's pace, -1 - 1.
  float pace_offset{0.f};
  //! When above 0, the steps that ratchet repeat every so many quarters
  //! instead of dividing the step by their ratchet.
  double repeat_time{0.};
  //! The time chooser's value: {seconds, 0} or {whole notes, synced}.
  ossia::vec2f repeat_time_value{0.f, 0.f};
  double tempo{120.};
  //! A step's hits once multiplied, held to what a step can still tell apart.
  static constexpr int max_ratchet = 16;

  //! Musical position the step counters count from: where the pattern started
  //! playing, or switched. NaN until the first tick with musical time.
  double origin = std::numeric_limits<double>::quiet_NaN();
  //! Set by a transport: the next tick moves the step counters to where the
  //! playhead landed.
  bool transport_requested{};

  //! Step of the pattern's own grid, which the lanes without a length or a
  //! division of their own follow.
  int current = 0;
  //! Points of the pattern's own grid since the origin: `current` before the
  //! modulo, so that a new length starts from where the pattern is.
  int64_t main_played{};
  //! Set when the list changes under a playing pattern: lanes that are new or
  //! changed rate, and the pattern's own counter when its length or rate
  //! changed, join on the next tick where they would be had they always played.
  std::array<bool, Pattern::maxLanes> resync_lane{};
  bool resync_main{};
  int last = -1;
  uint8_t channel{1};
  // Channel the notes currently in flight were started on: a channel change
  // must not leave them stranded on the previous one.
  uint8_t in_flight_channel{1};
  bool release_pending{};
  bool mustStop{};

  static constexpr int max_lanes = Pattern::maxLanes;
  struct lane_state
  {
    //! Steps the lane has started since the pattern started playing.
    int64_t played{};
    //! A step scheduled ahead because it plays early, or -1.
    int64_t early{-1};
    //! Note the lane holds until its next step, or -1.
    int16_t held{-1};
    //! Position of the note-on of `held`: its release goes after it, even when
    //! the next step lands before it - a late step followed by an early one.
    double held_on{-std::numeric_limits<double>::infinity()};
    //! Index of the step playing now, counted like `played`, or -1.
    int64_t last{-1};
    //! Timing random of the note at step `shift_for`, drawn a step ahead: a
    //! note it moves early is scheduled from the step before.
    int64_t shift_for{-1};
    double shift{};
    //! Where on its grid the step `last` is, in quarters.
    double last_at{};
    //! The note held is slid into the next step: it strikes before the
    //! release, and the same note carries on.
    bool slid{};
    //! A control lane's last steps: whether each is on, and its value, drawn
    //! once so that its outlet and the notes it acts on agree.
    struct decided
    {
      int64_t n{-1};
      float value{};
    };
    std::array<decided, 2> decisions{};
  };
  std::array<lane_state, max_lanes> lanes{};

  //! At the same position, in this order: an accent is set before the notes
  //! it acts on, a slide changes once the note it glides into is struck.
  enum class event_kind : uint8_t
  {
    accent,
    note_off,
    note_on,
    slide
  };
  //! A message waiting for the tick its musical position falls in.
  struct event
  {
    double position{};
    //! Order it was pushed in: what goes out at the same position goes out in
    //! that order, which is the order the lanes meant.
    uint32_t order{};
    event_kind kind{};
    uint8_t note{};
    uint8_t velocity{};
    float value{};
  };
  //! Once half full, what is due goes out before the next grid point, so the
  //! queue holds at most the steps around the one playing: the one before
  //! (late offset and swing delay it by up to a step), this one and the next
  //! (scheduled early), with room to spare - each a note-on and a note-off per
  //! hit, and a release.
  static constexpr int max_events = 4 * max_lanes * (2 * max_ratchet + 1);
  std::array<event, max_events> queue{};
  int queue_size{};
  uint32_t next_order{};
  //! What a step without musical time sends, all at the same date.
  std::array<event, 2 * max_lanes> immediate{};
  int immediate_size{};

  //! The grid points of a tick, across the divisions the lanes walk.
  struct grid_point
  {
    double position{};
    int64_t offset{};
    int division{};
    //! A point of the pattern's own grid, which moves `current`.
    bool main{};
  };
  static constexpr int max_points = 2048;
  std::array<grid_point, max_points> points{};

  std::minstd_rand rng{0x5eed};

  //! The accent and slide lanes of the pattern playing.
  std::array<int16_t, max_lanes> control_lanes{};
  int control_lane_count{};

  //! What the GUI draws, written at the end of every tick and read by its
  //! refresh timer: the only state both threads touch, so the GUI never reads
  //! `patterns` while a new list is swapped in.
  struct display_state
  {
    std::atomic<int> pattern{-1};
    std::atomic<int> pending{-1};
    std::atomic<int> lane_count{0};
    std::array<std::atomic<int64_t>, max_lanes> lanes;
  };
  display_state shown;

  pattern_node()
  {
    for(auto& l : shown.lanes)
      l.store(-1, std::memory_order_relaxed);
    m_inlets.push_back(&pattern_select);
    m_inlets.push_back(&switch_quantification);
    m_inlets.push_back(&fill_in);
    m_inlets.push_back(&density_in);
    m_inlets.push_back(&velocity_in);
    m_inlets.push_back(&swing_in);
    m_inlets.push_back(&ratchet_in);
    m_inlets.push_back(&pace_in);
    m_inlets.push_back(&repeat_time_in);
    m_outlets.push_back(&out);
    m_outlets.push_back(&accent_out);
    m_outlets.push_back(&slide_out);
  }

  std::string label() const noexcept override { return "pattern_node"; }

  const Pattern& pattern() const noexcept { return patterns[current_pattern]; }

  //! Ask for a pattern: it takes effect at the next quantification point.
  void request_pattern(int idx) noexcept
  {
    if(patterns.empty())
      return;
    idx = std::clamp(idx, 0, int(patterns.size()) - 1);
    pending_pattern = (idx == current_pattern) ? -1 : idx;
  }

  void release_all(int64_t timestamp) noexcept
  {
    auto& mess = out.target<ossia::midi_port>()->messages;
    for(int note = 0; note < 128; note++)
    {
      if(in_flight[note] == 0)
        continue;
      mess.push_back(libremidi::from_midi1::note_off(in_flight_channel, note, 0));
      mess.back().timestamp = timestamp;
      in_flight[note] = 0;
    }

    // What was scheduled belongs to what is being released.
    queue_size = 0;
    for(auto& l : lanes)
    {
      l.held = -1;
      l.held_on = -std::numeric_limits<double>::infinity();
      l.early = -1;
      l.slid = false;
    }
  }

  void reset_lanes() noexcept
  {
    for(auto& l : lanes)
      l = lane_state{};
    main_played = 0;
    resync_lane.fill(false);
    resync_main = false;
  }

  //! Takes a new list, swapped in so that the old one is freed elsewhere.
  void replace_patterns(std::vector<Pattern>& list) noexcept
  {
    if(ossia::valid_index(current_pattern, patterns)
       && ossia::valid_index(current_pattern, list))
    {
      const Pattern& before = patterns[current_pattern];
      const Pattern& after = list[current_pattern];
      const int count_before = std::min(int(before.lanes.size()), max_lanes);
      const int count_after = std::min(int(after.lanes.size()), max_lanes);
      for(int i = 0; i < count_after; i++)
        if(i >= count_before
           || laneDivision(before, before.lanes[i]) != laneDivision(after, after.lanes[i]))
          resync_lane[i] = true;
      if(before.length != after.length || before.division != after.division)
        resync_main = true;
    }
    std::swap(patterns, list);
  }

  void set_channel(uint8_t c) noexcept
  {
    if(c == channel)
      return;
    channel = c;
    // Released from run(), where a timestamp inside the tick is available
    release_pending = true;
  }

  //! The controls, from the inlets or from the GUI: each one keeps its value
  //! when given something that is not a finite number, and is held to its
  //! range otherwise.
  static bool finite(float v) noexcept { return std::isfinite(v); }
  void set_switch_rate(float v) noexcept
  {
    if(finite(v))
      switch_rate = std::clamp(v, 0.f, 64.f);
  }
  void set_density(float v) noexcept
  {
    if(finite(v))
      density = std::clamp(v, 0.f, 1.f);
  }
  void set_velocity_scale(float v) noexcept
  {
    if(finite(v))
      velocity_scale = std::clamp(v, 0.f, 1.f);
  }
  void set_swing(float v) noexcept
  {
    if(finite(v))
      swing = std::clamp(v, 0.f, 1.f);
  }
  void set_ratchet_scale(float v) noexcept
  {
    if(finite(v))
      ratchet_scale = std::clamp(v, 1.f, 8.f);
  }
  void set_pace_offset(float v) noexcept
  {
    if(finite(v))
      pace_offset = std::clamp(v, -1.f, 1.f);
  }
  void set_repeat_time(ossia::vec2f v) noexcept
  {
    if(finite(v[0]) && finite(v[1]))
      repeat_time_value = {std::clamp(v[0], 0.f, 60.f), v[1]};
  }

  void read_controls() noexcept
  {
    if(auto& d = pattern_select->get_data(); !d.empty())
      request_pattern(ossia::convert<int>(d.back().value));
    if(auto& d = switch_quantification->get_data(); !d.empty())
      set_switch_rate(ossia::convert<float>(d.back().value));
    if(auto& d = fill_in->get_data(); !d.empty())
      fill = ossia::convert<bool>(d.back().value);
    if(auto& d = density_in->get_data(); !d.empty())
      set_density(ossia::convert<float>(d.back().value));
    if(auto& d = velocity_in->get_data(); !d.empty())
      set_velocity_scale(ossia::convert<float>(d.back().value));
    if(auto& d = swing_in->get_data(); !d.empty())
      set_swing(ossia::convert<float>(d.back().value));
    if(auto& d = ratchet_in->get_data(); !d.empty())
      set_ratchet_scale(ossia::convert<float>(d.back().value));
    if(auto& d = pace_in->get_data(); !d.empty())
      set_pace_offset(ossia::convert<float>(d.back().value));
    if(auto& d = repeat_time_in->get_data(); !d.empty())
      set_repeat_time(ossia::convert<ossia::vec2f>(d.back().value));
  }

  //! The playhead moved somewhere else: the next tick follows it.
  void transport() noexcept { transport_requested = true; }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    using namespace ossia;

    // Before every early return below: the inlets are cleared once the node has
    // run, so a request read later than this is a request lost.
    read_controls();
    if(tk.tempo > 0. && std::isfinite(tk.tempo))
      tempo = tk.tempo;
    update_repeat_time();

    const double samplesratio = st.modelToSamples();
    // The magnitude of the speed: dividing by a negative speed while rewinding
    // turns the tick offset into a negative timestamp, which every consumer
    // that windows on [tick_start; tick_start + frames[ drops - and a dropped
    // note-off is a note stuck forever.
    const double speed = tk.speed != 0. ? std::abs(tk.speed) : 1.;
    // The span the producer handed us wins over reconstructing it from the
    // offset, which the flick rounding can miss by a sample. Reconstruction
    // stays for the tokens that carry no span - and for a null speed, where
    // the model -> sample map is undefined and token_request asserts.
    const int64_t tick_start
        = tk.start_sample >= 0
              ? int64_t(tk.start_sample)
              : int64_t(std::floor(tk.offset.impl * samplesratio / speed));

    // Before the empty-tick check: the token requested by stop() is empty.
    if(mustStop)
    {
      release_all(tick_start);
      mustStop = false;
      transport_requested = false;
      return;
    }

    if(tk.model_read_duration() == 0_tv)
      return;

    if(tk.end_discontinuous)
    {
      // Stamping at 0 puts the message before the beginning of the tick as soon
      // as the interval does not start on a buffer boundary; every consumer
      // that windows on [tick_start; tick_start + frames[ then drops it, and
      // since in_flight is cleared here the note is never released again.
      release_all(tick_start);
      return;
    }

    if(release_pending)
    {
      release_all(tick_start);
      in_flight_channel = channel;
      release_pending = false;
    }

    // The list can be replaced while a switch is waiting on it.
    if(patterns.empty())
      return;
    current_pattern = std::clamp(current_pattern, 0, int(patterns.size()) - 1);
    if(pending_pattern >= int(patterns.size()))
      pending_pattern = -1;
    release_removed_lanes(tick_start);

    const bool rewinding = tk.backward();
    // Without musical time (no tempo anywhere) or going backwards, every lane
    // takes one step per point of the pattern's grid and plays it at once:
    // there is no musical position to schedule gates, ratchets or offsets at.
    const bool timed = !rewinding
                       && tk.musical_end_position > tk.musical_start_position;

    if(transport_requested)
    {
      transport_requested = false;
      release_all(tick_start);
      reposition(tk);
    }
    if(!std::isfinite(origin) && std::isfinite(tk.musical_start_position))
      origin = tk.musical_start_position;
    if(timed)
      resync(tk);

    constexpr auto never = std::numeric_limits<int64_t>::max();
    int64_t switch_at = -1;
    double switch_position{};
    if(pending_pattern >= 0)
    {
      const auto points = tk.get_quantification_dates(switch_rate);
      // Points come in tick order in both directions, so the first one is the
      // earliest the switch can happen in this tick.
      if(!points.empty())
      {
        switch_at = tk.physical_position(points.front().position, samplesratio);
        switch_position = points.front().position;
      }
    }

    // The two patterns can have different divisions, so the steps around the
    // switch are walked on their own grids rather than on a single one.
    play_steps(
        tk, tick_start, samplesratio, rewinding, timed, 0,
        switch_at < 0 ? never : switch_at);

    if(switch_at >= 0)
    {
      if(timed)
        emit_until(tk, tick_start, samplesratio, switch_position);
      apply_switch(tick_start + switch_at);
      origin = switch_position;
      play_steps(tk, tick_start, samplesratio, rewinding, timed, switch_at, never);
    }

    if(timed)
      emit_until(tk, tick_start, samplesratio, tk.musical_end_position);
    publish();
  }

  void publish() noexcept
  {
    const int count
        = patterns.empty() ? 0 : std::min(int(pattern().lanes.size()), max_lanes);
    for(int i = 0; i < count; i++)
      shown.lanes[i].store(lanes[i].last, std::memory_order_relaxed);
    shown.lane_count.store(count, std::memory_order_relaxed);
    shown.pattern.store(patterns.empty() ? -1 : current_pattern, std::memory_order_relaxed);
    shown.pending.store(pending_pattern, std::memory_order_relaxed);
  }

  //! Plays the steps of the current pattern whose offset in the tick falls in
  //! [from_offset; to_offset[.
  void play_steps(
      const ossia::token_request& tk, int64_t tick_start, double samplesratio,
      bool rewinding, bool timed, int64_t from_offset, int64_t to_offset) noexcept
  {
    const Pattern& pat = pattern();
    // The model keeps divisions within range; anything past it would walk a
    // thousand points a tick.
    if(pat.length <= 0 || pat.division <= 0 || pat.division > Pattern::maxDivision)
      return;
    if(current < 0 || current >= pat.length)
      current = 0;
    const int lane_count = std::min(int(pat.lanes.size()), max_lanes);
    control_lane_count = 0;
    for(int i = 0; i < lane_count; i++)
      if(isControlLane(pat.lanes[i]))
        control_lanes[control_lane_count++] = int16_t(i);

    // Every point of the tick, not just the first: a tick covers more than one
    // step as soon as the division is small, the buffer large or the tempo
    // high. Rewinding, the points come in decreasing musical order, and the
    // pattern walks back the same way. Lanes with a division of their own walk
    // their own grid, but in time order with the others: what is due goes out
    // before each point, so nothing pushed later can fall before it.
    int count = 0;
    const auto collect = [&](int division, bool main) {
      tk.for_each_quantification_date(division, [&](const auto& q) {
        // Through the tick's one musical-position -> sample map, not through
        // the point's date: the date is truncated to a whole flick, so
        // flooring it into a sample rounds twice and puts the step a sample
        // before the metronome click on the same bar line.
        const int64_t offset = tk.physical_position(q.position, samplesratio);
        if(offset >= from_offset && offset < to_offset && count < max_points)
          points[count++] = {q.position, offset, division, main};
      });
    };
    collect(pat.division, true);
    if(timed)
    {
      std::array<bool, Pattern::maxDivision + 1> walked{};
      walked[std::min(pat.division, Pattern::maxDivision)] = true;
      for(int i = 0; i < lane_count; i++)
      {
        const int div = laneDivision(pat, pat.lanes[i]);
        if(div > 0 && div <= Pattern::maxDivision && !walked[div])
        {
          walked[div] = true;
          collect(div, false);
        }
      }
      std::sort(
          points.begin(), points.begin() + count,
          [](const grid_point& a, const grid_point& b) {
        if(a.position != b.position)
          return a.position < b.position;
        return a.main > b.main;
      });
    }

    const bool solo = anySolo(pat);
    for(int k = 0; k < count; k++)
    {
      const grid_point& q = points[k];
      // The points come in time order, so what goes out here can never be
      // later than what the points still to come push.
      if(timed && queue_size >= max_events / 2)
        emit_until(tk, tick_start, samplesratio, q.position);

      // The accent and slide lanes first, so that the notes they act on
      // find them on this step.
      if(!q.main)
      {
        for(int first = 1; first >= 0; first--)
          for(int i = 0; i < lane_count; i++)
            if(isControlLane(pat.lanes[i]) == bool(first)
               && laneDivision(pat, pat.lanes[i]) == q.division)
              play_lane(
                  pat, i, q.position, 4. / q.division, false, true, solo,
                  tick_start + q.offset);
        continue;
      }

      // Forward, the step about to play is the current one and the next tick
      // plays the one after. Rewinding mirrors that: step back first, so going
      // out and back over the same ground crosses the same steps.
      if(rewinding)
      {
        current = (current + pat.length - 1) % pat.length;
        main_played = std::max<int64_t>(0, main_played - 1);
      }
      last = current;
      in_flight_channel = channel;

      for(int first = 1; first >= 0; first--)
        for(int i = 0; i < lane_count; i++)
        {
          if(isControlLane(pat.lanes[i]) != bool(first))
            continue;
          if(timed && laneDivision(pat, pat.lanes[i]) != pat.division)
            continue;
          play_lane(
              pat, i, q.position, 4. / pat.division, rewinding, timed, solo,
              tick_start + q.offset);
        }
      flush_immediate(tick_start + q.offset);

      if(!rewinding)
      {
        current = (current + 1) % pat.length;
        main_played++;
      }
    }
  }

  //! Points of a grid of `division` per whole note in [from; to[, the grid
  //! starting again at every bar line: bars of `bar` quarters, one of them at
  //! `bar_line`. The same points get_quantification_dates() walks.
  static int64_t points_between(
      double from, double to, double bar_line, double bar, int division) noexcept
  {
    if(!(bar > 0.) || division <= 0 || !(to > from))
      return 0;
    const double unit = 4. / division;
    const auto per_bar = int64_t(std::ceil(bar / unit - 1e-9));
    const auto before = [&](double x) {
      const double rel = x - bar_line;
      const double bars = std::floor(rel / bar + 1e-12);
      const double within = std::max(0., rel - bars * bar);
      return int64_t(bars) * per_bar
             + std::min(per_bar, int64_t(std::ceil(within / unit - 1e-9)));
    };
    return before(to) - before(from);
  }

  //! After a transport: the counters as if the pattern had played from its
  //! origin up to where the tick starts.
  void reposition(const ossia::token_request& tk) noexcept
  {
    reset_lanes();
    current = 0;
    const double at = tk.musical_start_position;
    if(!std::isfinite(origin) || !std::isfinite(at) || patterns.empty())
    {
      origin = std::numeric_limits<double>::quiet_NaN();
      return;
    }
    const auto& sig = tk.signature;
    const double bar = (sig.upper > 0 && sig.lower > 0)
                           ? 4. * sig.upper / sig.lower
                           : 4.;
    const Pattern& pat = pattern();
    if(pat.length <= 0 || pat.division <= 0)
      return;
    const double bar_line = tk.musical_start_last_bar;
    const int64_t steps = points_between(origin, at, bar_line, bar, pat.division);
    main_played = steps;
    current = int(steps % pat.length);
    const int lane_count = std::min(int(pat.lanes.size()), max_lanes);
    for(int i = 0; i < lane_count; i++)
      lanes[i].played
          = points_between(origin, at, bar_line, bar, laneDivision(pat, pat.lanes[i]));
  }

  //! Puts the lanes flagged by replace_patterns where they would be had they
  //! always played: a lane on the pattern's grid on its count, another on the
  //! points of its own grid since the origin. What they hold carries on.
  void resync(const ossia::token_request& tk) noexcept
  {
    if(patterns.empty())
      return;
    const Pattern& pat = pattern();
    if(pat.length <= 0 || pat.division <= 0)
      return;
    if(resync_main)
    {
      resync_main = false;
      current = int(main_played % pat.length);
    }
    const auto& sig = tk.signature;
    const double bar = (sig.upper > 0 && sig.lower > 0)
                           ? 4. * sig.upper / sig.lower
                           : 4.;
    const int lane_count = std::min(int(pat.lanes.size()), max_lanes);
    for(int i = 0; i < lane_count; i++)
    {
      if(!resync_lane[i])
        continue;
      resync_lane[i] = false;
      const int div = laneDivision(pat, pat.lanes[i]);
      auto& ls = lanes[i];
      ls.played = div == pat.division || !std::isfinite(origin)
                      ? main_played
                      : points_between(
                          origin, tk.musical_start_position, tk.musical_start_last_bar,
                          bar, div);
      ls.early = -1;
    }
    for(int i = lane_count; i < max_lanes; i++)
      resync_lane[i] = false;
  }

  //! A lane that is gone cannot release what it held: release it here.
  void release_removed_lanes(int64_t date) noexcept
  {
    const int lane_count = std::min(int(pattern().lanes.size()), max_lanes);
    for(int i = lane_count; i < max_lanes; i++)
    {
      auto& ls = lanes[i];
      if(ls.held >= 0)
        emit({0., 0, event_kind::note_off, uint8_t(ls.held), 0, 0.f}, date);
      ls = lane_state{};
    }
  }

  //! Lane `i` reaches a point of its grid at musical position `position`, in
  //! quarters; `unit` is the length of its step in quarters.
  void play_lane(
      const Pattern& pat, int i, double position, double unit, bool rewinding,
      bool timed, bool solo, int64_t date) noexcept
  {
    const Lane& l = pat.lanes[i];
    auto& ls = lanes[i];
    const int len = laneLength(pat, l);
    if(len <= 0)
      return;

    if(rewinding)
      ls.played = std::max<int64_t>(0, ls.played - 1);
    const int64_t n = ls.played;
    const int step = int(n % len);
    ls.last = n;
    ls.last_at = position;
    if(!rewinding)
      ls.played++;

    if(!timed)
    {
      play_step(pat, i, step, n / len, 0., 0., 0., false, solo, date);
      return;
    }

    // Scheduled from the step before, because it plays early.
    if(ls.early != n)
    {
      const double shift = ls.shift_for == n ? ls.shift : note_shift(l, step);
      play_step(
          pat, i, step, n / len, position,
          position + (std::max(0., timing(l, step, shift)) + swing_delay(step)) * unit,
          unit, true, solo, date);
    }
    ls.early = -1;

    // The next step, if it plays early, has to be scheduled now: by the time
    // its own grid point comes, its notes are already due.
    const int64_t next = n + 1;
    const int next_step = int(next % len);
    ls.shift = note_shift(l, next_step);
    ls.shift_for = next;
    if(const double t = timing(l, next_step, ls.shift); t < 0.)
    {
      play_step(
          pat, i, next_step, next / len, position + unit,
          position + (1. + t + swing_delay(next_step)) * unit, unit, true, solo, date);
      ls.early = next;
    }
  }

  //! The timing random a whole note moves by, in steps: none when each hit
  //! draws its own.
  double note_shift(const Lane& l, int step) noexcept
  {
    const float r = stepParams(l, step).offsetRandom;
    if(r <= 0.f || (l.randomPerHit & Lane::TimingPerHit))
      return 0.;
    return std::uniform_real_distribution<double>{-r, r}(rng);
  }

  //! Where a step's note starts, in steps from its grid point.
  static double timing(const Lane& l, int step, double shift) noexcept
  {
    return std::clamp(stepParams(l, step).offset + shift, -0.5, 0.5);
  }

  void update_repeat_time() noexcept
  {
    const auto [x, mode] = repeat_time_value;
    if(!(x > 0.f))
      repeat_time = 0.;
    else if(mode != 0.f)
      repeat_time = 4. * x;
    else
      repeat_time = x * tempo / 60.;
  }

  //! Where a step's hits start and end, in fractions of the step. Its
  //! ratchet, times the Ratchet input, divides the step; with a repeat time,
  //! the steps that ratchet repeat at that interval instead, divided by the
  //! Ratchet input. The last gap may be cut short by the next step. The pace
  //! then bends the spacing: positive starts wide and tightens, negative the
  //! other way round. `gate_of(k)` is the gate of hit k.
  struct hit
  {
    double on, off;
  };
  template <typename Gate>
  int hits(
      const StepParams& p, double unit, bool scheduled, std::array<hit, max_ratchet>& out,
      Gate&& gate_of) const noexcept
  {
    const auto gate = [&](int k) { return std::clamp(gate_of(k), 0.05f, 1.f); };
    if(!scheduled || unit <= 0.)
    {
      out[0] = {0., scheduled ? gate(0) : 1.};
      return 1;
    }

    double gap = 1. / (std::max<int>(1, p.ratchet) * ratchet_scale);
    if(repeat_time > 0. && p.ratchet > 1)
      gap = repeat_time / ratchet_scale / unit;
    const int n = std::clamp(int(std::ceil(1. / gap - 1e-6)), 1, max_ratchet);

    const double pace = std::clamp(p.pace + pace_offset, -1.f, 1.f);
    const double exponent = std::exp2(-2. * pace);
    const auto bend = [&](double x) { return std::pow(std::min(x, 1.), exponent); };
    for(int k = 0; k < n; k++)
    {
      const double on = bend(k * gap);
      const double next = k + 1 < n ? bend((k + 1) * gap) : 1.;
      out[k] = {on, on + gate(k) * (next - on)};
    }
    return n;
  }

  //! Every other step is delayed, by up to half a step.
  double swing_delay(int step) const noexcept
  {
    return (step % 2 == 1) ? swing * 0.5 : 0.;
  }

  bool condition_holds(const StepParams& p, int64_t pass) const noexcept
  {
    switch(p.condition)
    {
      case Condition::Always:
        return true;
      case Condition::Fill:
        return fill;
      case Condition::NotFill:
        return !fill;
      case Condition::First:
        return pass == 0;
      case Condition::NotFirst:
        return pass != 0;
      case Condition::Cycle:
        return p.cycleB > 0 && (pass % p.cycleB) == (p.cycleA - 1);
    }
    return true;
  }

  //! Step `n` of control lane `i`: 0 when it does not play, otherwise its
  //! accent amount, or its glide time for a slide. Drawn once per step.
  float control_value(const Pattern& pat, int i, int64_t n) noexcept
  {
    auto& ls = lanes[i];
    for(const auto& d : ls.decisions)
      if(d.n == n)
        return d.value;

    const Lane& l = pat.lanes[i];
    const int len = laneLength(pat, l);
    if(len <= 0 || n < 0)
      return 0.f;
    const int step = int(n % len);
    const StepParams& p = stepParams(l, step);
    float v = 0.f;
    // The solo of a note lane leaves the accents and slides on it.
    if(audibleNote(pat, l, step, false) != Note::Rest && condition_holds(p, n / len))
    {
      const float chance = p.chance * density;
      if(chance >= 1.f || std::uniform_real_distribution<float>{0.f, 1.f}(rng) < chance)
      {
        if(l.note == accentLane)
        {
          const float spread
              = p.velocityRandom > 0.f
                    ? std::uniform_real_distribution<float>{
                          -p.velocityRandom, p.velocityRandom}(rng)
                    : 0.f;
          v = std::clamp(p.velocity + spread, 0.f, 1.f) * std::clamp(l.velocity, 0.f, 1.f);
        }
        else
        {
          v = std::clamp(p.gate, 0.05f, 1.f);
        }
      }
    }
    auto& slot = ls.decisions[0].n < ls.decisions[1].n ? ls.decisions[0] : ls.decisions[1];
    slot = {n, v};
    return v;
  }

  //! What the control lanes do to a note on the grid at `grid`: the steps of
  //! theirs it falls in.
  struct step_controls
  {
    float accent{};
    bool slide{};
  };
  step_controls controls_at(const Pattern& pat, double grid, bool scheduled) noexcept
  {
    step_controls c;
    for(int k = 0; k < control_lane_count; k++)
    {
      const int i = control_lanes[k];
      const Lane& l = pat.lanes[i];
      const auto& ls = lanes[i];
      // Played before the note lanes on every point: without a position, the
      // step it played last is the one.
      int64_t n = ls.last;
      if(scheduled && ls.last >= 0)
      {
        const double unit = 4. / laneDivision(pat, l);
        n += int64_t(std::floor((grid - ls.last_at) / unit + 1e-6));
      }
      else if(ls.last < 0)
        n = ls.played;
      const float v = control_value(pat, i, n);
      if(l.note == accentLane)
        c.accent = std::max(c.accent, v);
      else
        c.slide |= v > 0.f;
    }
    return c;
  }

  //! Plays step `step` of lane `i` at musical position `at`, its point on the
  //! grid being `grid`. When `scheduled` is false everything goes out at once,
  //! at `date`.
  void play_step(
      const Pattern& pat, int i, int step, int64_t pass, double grid, double at,
      double unit, bool scheduled, bool solo, int64_t date) noexcept
  {
    const Lane& l = pat.lanes[i];
    auto& ls = lanes[i];
    const StepParams& p = stepParams(l, step);

    const auto push = [&](double position, event_kind k, int note, int velocity,
                          float value = 0.f) {
      push_event(
          {position, 0, k, uint8_t(note), uint8_t(velocity), value}, scheduled, date);
    };

    if(isControlLane(l))
    {
      const auto kind = l.note == accentLane ? event_kind::accent : event_kind::slide;
      const float v = control_value(pat, i, pass * laneLength(pat, l) + step);
      push(at, kind, 0, 0, v);
      // An accent's gate ends it within the step.
      if(kind == event_kind::accent && v > 0.f && scheduled && p.gate < 1.f)
        push(at + std::max(p.gate, 0.05f) * unit, kind, 0, 0, 0.f);
      return;
    }
    if(l.note > 127)
      return;

    Note kind = audibleNote(pat, l, step, solo);
    if(kind != Note::Rest)
    {
      if(!condition_holds(p, pass))
        kind = Note::Rest;
      else if(const float chance = p.chance * density; chance < 1.f)
        if(std::uniform_real_distribution<float>{0.f, 1.f}(rng) >= chance)
          kind = Note::Rest;
    }
    const step_controls controls = control_lane_count > 0 && kind != Note::Rest
                                       ? controls_at(pat, grid, scheduled)
                                       : step_controls{};

    // Pitch and velocity spreads: drawn once for the whole note, or by each
    // hit for itself when the lane says so.
    const auto draw_pitch = [&] {
      return p.pitchRandom > 0
                 ? std::uniform_int_distribution<int>{-p.pitchRandom, p.pitchRandom}(rng)
                 : 0;
    };
    const auto draw_velocity = [&] {
      return p.velocityRandom > 0.f
                 ? std::uniform_real_distribution<float>{
                       -p.velocityRandom, p.velocityRandom}(rng)
                 : 0.f;
    };
    const bool pitch_per_hit = l.randomPerHit & Lane::PitchPerHit;
    const bool velocity_per_hit = l.randomPerHit & Lane::VelocityPerHit;
    const int note_pitch = kind != Note::Rest && !pitch_per_hit ? draw_pitch() : 0;
    const float note_velocity
        = kind != Note::Rest && !velocity_per_hit ? draw_velocity() : 0.f;
    const auto to_note = [&](int pitch) {
      return std::clamp(int(l.note) + p.pitch + pitch, 0, 127);
    };
    // An accent takes the step's velocity part of the way to the top.
    const auto to_velocity = [&](float spread) {
      float v = std::clamp(p.velocity + spread, 0.f, 1.f);
      v += controls.accent * (1.f - v);
      return std::clamp(
          int(std::lround(v * l.velocity * velocity_scale * 127.f)), 1, 127);
    };
    // The note and velocity of hit k.
    const auto strike = [&](int k) {
      return std::make_pair(
          to_note(pitch_per_hit ? draw_pitch() : note_pitch),
          to_velocity(velocity_per_hit ? draw_velocity() : note_velocity));
    };
    const int note = to_note(note_pitch);

    const bool gate_per_hit = l.randomPerHit & Lane::GatePerHit;
    const auto draw_gate = [&] {
      return p.gateRandom > 0.f ? std::uniform_real_distribution<float>{
                                      -p.gateRandom, p.gateRandom}(rng)
                                : 0.f;
    };
    const float note_gate = kind != Note::Rest && !gate_per_hit ? draw_gate() : 0.f;
    const auto gate_of
        = [&](int) { return p.gate + (gate_per_hit ? draw_gate() : note_gate); };

    // Timing spread of each hit, when each hit draws its own: within its own
    // gap, and never before the step for the first one.
    const bool timing_per_hit
        = scheduled && p.offsetRandom > 0.f && (l.randomPerHit & Lane::TimingPerHit);
    const auto hit_shift = [&](const std::array<hit, max_ratchet>& h, int k, int n) {
      if(!timing_per_hit)
        return 0.;
      const double gap = (k + 1 < n ? h[k + 1].on : 1.) - h[k].on;
      const double r = p.offsetRandom * gap;
      const double d = std::uniform_real_distribution<double>{-r, r}(rng);
      return k == 0 ? std::max(0., d) : d;
    };

    constexpr auto inf = std::numeric_limits<double>::infinity();
    // Released where this step starts, or right after it was struck when this
    // step starts first.
    const auto release_held = [&] {
      if(ls.held >= 0)
        push(
            std::max(at, std::nextafter(ls.held_on, inf)), event_kind::note_off, ls.held,
            0);
      ls.held = -1;
    };
    // Slid into: struck at `on` first, the held note released right after;
    // the same note just carries on.
    const bool slid = ls.slid && ls.held >= 0;
    ls.slid = false;
    const auto glide_to = [&](double on, int hit_note, int hit_velocity) {
      if(ls.held != hit_note)
      {
        push(on, event_kind::note_on, hit_note, hit_velocity);
        push(
            std::nextafter(std::max(on, ls.held_on), inf), event_kind::note_off, ls.held,
            0);
        ls.held_on = on;
      }
      ls.held = hit_note;
    };

    switch(kind)
    {
      case Note::Rest:
        release_held();
        break;

      case Note::Legato: {
        // The first hit ties on from the step before; a ratchet strikes the
        // others, each released after its gate, and the last is held into the
        // next step unless the gate ends it first.
        std::array<hit, max_ratchet> h;
        const int n = hits(p, unit, scheduled, h, gate_of);
        // With a pitch drawn by each hit, the tie holds whatever was struck.
        if(slid)
          glide_to(at, note, strike(0).second);
        else if(ls.held < 0 || (ls.held != note && !pitch_per_hit))
        {
          release_held();
          push(at, event_kind::note_on, note, strike(0).second);
          ls.held = note;
          ls.held_on = at;
        }
        int sounding = ls.held;
        double off = at + h[0].off * unit;
        for(int k = 1; k < n; k++)
        {
          const double shift = hit_shift(h, k, n);
          const auto [hit_note, hit_velocity] = strike(k);
          push(off, event_kind::note_off, sounding, 0);
          push(at + (h[k].on + shift) * unit, event_kind::note_on, hit_note, hit_velocity);
          ls.held = sounding = hit_note;
          ls.held_on = at + (h[k].on + shift) * unit;
          off = at + (h[k].off + shift) * unit;
        }
        // A gate under a step ends the tied note there, unless it slides on.
        if(h[n - 1].off < 1. && !controls.slide)
        {
          push(off, event_kind::note_off, sounding, 0);
          ls.held = -1;
        }
        break;
      }

      case Note::Note: {
        if(!slid)
          release_held();

        std::array<hit, max_ratchet> h;
        const int n = hits(p, unit, scheduled, h, gate_of);
        for(int k = 0; k < n; k++)
        {
          const double shift = hit_shift(h, k, n);
          const auto [hit_note, hit_velocity] = strike(k);
          const double on = at + (h[k].on + shift) * unit;
          if(k == 0 && slid)
            glide_to(on, hit_note, hit_velocity);
          else
            push(on, event_kind::note_on, hit_note, hit_velocity);
          // The last hit with a full gate, or sliding on, holds until the
          // next step.
          if(k + 1 == n && (h[k].off >= 1. || controls.slide))
          {
            if(!(k == 0 && slid && ls.held_on < on))
              ls.held_on = on;
            ls.held = hit_note;
          }
          else
          {
            push(at + (h[k].off + shift) * unit, event_kind::note_off, hit_note, 0);
            if(k == 0 && slid)
              ls.held = -1;
          }
        }
        break;
      }
    }
    ls.slid = controls.slide && ls.held >= 0;
  }

  //! The queue keeps room for releases: when it is that full, a note that
  //! could not be released later is not struck at all.
  static constexpr int release_reserve = 2 * max_lanes;
  void push_event(event e, bool scheduled, int64_t date) noexcept
  {
    e.order = next_order++;
    if(!scheduled)
    {
      if(immediate_size < int(immediate.size()))
        immediate[immediate_size++] = e;
      return;
    }
    // A position that is not a number would never come due.
    if(!std::isfinite(e.position))
      return;
    if(e.kind == event_kind::note_on && queue_size >= max_events - release_reserve)
      return;
    if(queue_size < max_events)
      queue[queue_size++] = e;
  }

  //! Every lane's releases go before any lane strikes.
  void flush_immediate(int64_t date) noexcept
  {
    std::sort(
        immediate.begin(), immediate.begin() + immediate_size,
        [](const event& a, const event& b) {
      if(a.kind != b.kind)
        return a.kind < b.kind;
      return a.order < b.order;
    });
    for(int i = 0; i < immediate_size; i++)
      emit(immediate[i], date);
    immediate_size = 0;
  }

  //! Sends what is due before musical position `end`, in order: at the same
  //! position a note-off goes before a note-on, so that a note restarted on
  //! the step that ends it is heard; then in the order they were pushed.
  void emit_until(
      const ossia::token_request& tk, int64_t tick_start, double samplesratio,
      double end) noexcept
  {
    const auto due_end = std::partition(
        queue.begin(), queue.begin() + queue_size,
        [end](const event& e) { return e.position < end - 1e-9; });
    std::sort(queue.begin(), due_end, [](const event& a, const event& b) {
      if(a.position != b.position)
        return a.position < b.position;
      if(a.kind != b.kind)
        return a.kind < b.kind;
      return a.order < b.order;
    });

    // Late arrivals are stamped at the start of the tick rather than before it,
    // where every consumer would drop them.
    for(auto it = queue.begin(); it != due_end; ++it)
      emit(
          *it, tick_start
                   + tk.physical_position(
                       std::max(it->position, tk.musical_start_position), samplesratio));

    const int remaining = int((queue.begin() + queue_size) - due_end);
    std::move(due_end, queue.begin() + queue_size, queue.begin());
    queue_size = remaining;
  }

  void emit(const event& e, int64_t date) noexcept
  {
    auto& mess = out.target<ossia::midi_port>()->messages;
    switch(e.kind)
    {
      case event_kind::note_off:
        if(e.note < 128 && in_flight[e.note] > 0 && --in_flight[e.note] == 0)
        {
          mess.push_back(libremidi::from_midi1::note_off(in_flight_channel, e.note, 0));
          mess.back().timestamp = date;
        }
        break;
      case event_kind::note_on:
        if(e.note >= 128)
          break;
        // Struck again while another lane holds it: the voice restarts.
        if(in_flight[e.note] > 0)
        {
          mess.push_back(libremidi::from_midi1::note_off(in_flight_channel, e.note, 0));
          mess.back().timestamp = date;
        }
        mess.push_back(libremidi::from_midi1::note_on(channel, e.note, e.velocity));
        mess.back().timestamp = date;
        if(in_flight[e.note] < 255)
          in_flight[e.note]++;
        in_flight_channel = channel;
        break;
      case event_kind::accent:
        accent_out->write_value(e.value, date);
        break;
      case event_kind::slide:
        slide_out->write_value(e.value, date);
        break;
    }
  }

  void apply_switch(int64_t date) noexcept
  {
    // Everything in flight belongs to the pattern going away: the new one only
    // ever releases the notes its own lanes carry, so the rest would hang.
    release_all(date);
    in_flight_channel = channel;

    current_pattern = pending_pattern;
    pending_pattern = -1;
    current = 0;
    reset_lanes();
  }

  // Writing to the outlet from here would be pointless: this runs outside of a
  // tick, and init_outlet() clears every outlet before the node runs again.
  void all_notes_off() noexcept override { mustStop = true; }
};

//! node_process::stop() only calls all_notes_off(), whose messages nothing
//! would ever read. Request a tick so the node can flush them itself, the way
//! ossia::nodes::midi_node_process does.
class pattern_node_process final : public ossia::node_process
{
public:
  using ossia::node_process::node_process;

  void stop() override
  {
    auto& n = *static_cast<pattern_node*>(node.get());
    n.request(ossia::token_request{});
    n.mustStop = true;
  }

  void transport_impl(ossia::time_value date) override
  {
    static_cast<pattern_node*>(node.get())->transport();
  }
};
}
