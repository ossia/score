#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <Patternist/PatternModel.hpp>
#include <libremidi/ump_events.hpp>

#include <algorithm>
#include <array>
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
  ossia::flat_set<uint8_t> in_flight;

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

  //! Step of the pattern's own grid, which the lanes without a length or a
  //! division of their own follow.
  int current = 0;
  int last = -1;
  uint8_t channel{1};
  // Channel the notes currently in flight were started on: a channel change
  // must not leave them stranded on the previous one.
  uint8_t in_flight_channel{1};
  bool release_pending{};
  bool mustStop{};

  static constexpr int max_lanes = 128;
  struct lane_state
  {
    //! Steps the lane has started since the pattern started playing.
    int64_t played{};
    //! A step scheduled ahead because it plays early, or -1.
    int64_t early{-1};
    //! Note the lane holds until its next step, or -1.
    int16_t held{-1};
    //! Index of the step playing now, counted like `played`, or -1.
    int64_t last{-1};
  };
  std::array<lane_state, max_lanes> lanes{};

  enum class event_kind : uint8_t
  {
    note_off,
    note_on,
    accent,
    slide
  };
  //! A message waiting for the tick its musical position falls in.
  struct event
  {
    double position{};
    event_kind kind{};
    uint8_t note{};
    uint8_t velocity{};
    float value{};
  };
  static constexpr int max_events = 1024;
  std::array<event, max_events> queue{};
  int queue_size{};
  //! What a step without musical time sends, all at the same date.
  std::array<event, 2 * max_lanes> immediate{};
  int immediate_size{};

  std::minstd_rand rng{0x5eed};

  pattern_node()
  {
    in_flight.reserve(32);
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
    for(uint8_t note : in_flight)
    {
      mess.push_back(libremidi::from_midi1::note_off(in_flight_channel, note, 0));
      mess.back().timestamp = timestamp;
    }
    in_flight.clear();

    // What was scheduled belongs to what is being released.
    queue_size = 0;
    for(auto& l : lanes)
    {
      l.held = -1;
      l.early = -1;
    }
  }

  void reset_lanes() noexcept
  {
    for(auto& l : lanes)
      l = lane_state{};
  }

  void set_channel(uint8_t c) noexcept
  {
    if(c == channel)
      return;
    channel = c;
    // Released from run(), where a timestamp inside the tick is available
    release_pending = true;
  }

  void read_controls() noexcept
  {
    if(auto& d = pattern_select->get_data(); !d.empty())
      request_pattern(ossia::convert<int>(d.back().value));
    if(auto& d = switch_quantification->get_data(); !d.empty())
      switch_rate = ossia::convert<float>(d.back().value);
    if(auto& d = fill_in->get_data(); !d.empty())
      fill = ossia::convert<bool>(d.back().value);
    if(auto& d = density_in->get_data(); !d.empty())
      density = std::clamp(ossia::convert<float>(d.back().value), 0.f, 1.f);
    if(auto& d = velocity_in->get_data(); !d.empty())
      velocity_scale = std::clamp(ossia::convert<float>(d.back().value), 0.f, 1.f);
    if(auto& d = swing_in->get_data(); !d.empty())
      swing = std::clamp(ossia::convert<float>(d.back().value), 0.f, 1.f);
    if(auto& d = ratchet_in->get_data(); !d.empty())
      ratchet_scale = std::clamp(ossia::convert<float>(d.back().value), 1.f, 8.f);
    if(auto& d = pace_in->get_data(); !d.empty())
      pace_offset = std::clamp(ossia::convert<float>(d.back().value), -1.f, 1.f);
    if(auto& d = repeat_time_in->get_data(); !d.empty())
      repeat_time_value = ossia::convert<ossia::vec2f>(d.back().value);
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    using namespace ossia;

    // Before every early return below: the inlets are cleared once the node has
    // run, so a request read later than this is a request lost.
    read_controls();
    if(tk.tempo > 0.)
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

    const bool rewinding = tk.backward();
    // Without musical time (no tempo anywhere) or going backwards, every lane
    // takes one step per point of the pattern's grid and plays it at once:
    // there is no musical position to schedule gates, ratchets or offsets at.
    const bool timed = !rewinding
                       && tk.musical_end_position > tk.musical_start_position;

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
      play_steps(tk, tick_start, samplesratio, rewinding, timed, switch_at, never);
    }

    if(timed)
      emit_until(tk, tick_start, samplesratio, tk.musical_end_position);
  }

  //! Plays the steps of the current pattern whose offset in the tick falls in
  //! [from_offset; to_offset[.
  void play_steps(
      const ossia::token_request& tk, int64_t tick_start, double samplesratio,
      bool rewinding, bool timed, int64_t from_offset, int64_t to_offset) noexcept
  {
    const Pattern& pat = pattern();
    if(pat.length <= 0 || pat.division <= 0)
      return;
    if(current < 0 || current >= pat.length)
      current = 0;
    const int lane_count = std::min(int(pat.lanes.size()), max_lanes);

    // All of them, not just the first: a tick covers more than one step as soon
    // as the division is small, the buffer large or the tempo high, and the
    // single-date version would silently drop every step but one.
    // get_quantification_dates walks the grid in tick order, so rewinding it
    // hands back the steps the tick crosses in decreasing musical order. Walk
    // the pattern the same way, or the sequence marches on while the timeline
    // runs the other way.
    for(const auto& q : tk.get_quantification_dates(pat.division))
    {
      // Through the tick's one musical-position -> sample map, not through the
      // point's date: the date is truncated to a whole flick, so flooring it
      // into a sample rounds twice and puts the step a sample before the
      // metronome click on the same bar line.
      const int64_t offset = tk.physical_position(q.position, samplesratio);
      if(offset < from_offset || offset >= to_offset)
        continue;

      // Forward, the step about to play is the current one and the next tick
      // plays the one after. Rewinding mirrors that: step back first, so going
      // out and back over the same ground crosses the same steps.
      if(rewinding)
        current = (current + pat.length - 1) % pat.length;
      last = current;
      in_flight_channel = channel;

      for(int i = 0; i < lane_count; i++)
      {
        const Lane& l = pat.lanes[i];
        if(timed && laneDivision(pat, l) != pat.division)
          continue;
        play_lane(
            pat, i, q.position, 4. / pat.division, rewinding, timed,
            tick_start + offset);
      }
      flush_immediate(tick_start + offset);

      if(!rewinding)
        current = (current + 1) % pat.length;
    }

    // Lanes with a division of their own walk their own grid.
    if(!timed)
      return;
    for(int i = 0; i < lane_count; i++)
    {
      const Lane& l = pat.lanes[i];
      const int div = laneDivision(pat, l);
      if(div == pat.division)
        continue;

      // Several lanes can share a division: each walks it on its own, the
      // points are cheap.
      for(const auto& q : tk.get_quantification_dates(div))
      {
        const int64_t offset = tk.physical_position(q.position, samplesratio);
        if(offset < from_offset || offset >= to_offset)
          continue;
        play_lane(pat, i, q.position, 4. / div, false, true, tick_start + offset);
      }
    }
  }

  //! Lane `i` reaches a point of its grid at musical position `position`, in
  //! quarters; `unit` is the length of its step in quarters.
  void play_lane(
      const Pattern& pat, int i, double position, double unit, bool rewinding,
      bool timed, int64_t date) noexcept
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
    if(!rewinding)
      ls.played++;

    if(!timed)
    {
      play_step(pat, i, step, n / len, 0., 0., false, date);
      return;
    }

    // Scheduled from the step before, because it plays early.
    if(ls.early != n)
    {
      const auto& p = stepParams(l, step);
      play_step(
          pat, i, step, n / len,
          position + (std::max(0.f, p.offset) + swing_delay(step)) * unit, unit, true,
          date);
    }
    ls.early = -1;

    // The next step, if it plays early, has to be scheduled now: by the time
    // its own grid point comes, its notes are already due.
    const int64_t next = n + 1;
    const int next_step = int(next % len);
    const auto& np = stepParams(l, next_step);
    if(np.offset < 0.f)
    {
      play_step(
          pat, i, next_step, next / len,
          position + (1. + np.offset + swing_delay(next_step)) * unit, unit, true, date);
      ls.early = next;
    }
  }

  //! Every other step is delayed, by up to half a step.
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
  //! other way round.
  struct hit
  {
    double on, off;
  };
  int hits(
      const StepParams& p, double unit, bool scheduled,
      std::array<hit, max_ratchet>& out) const noexcept
  {
    const double gate = std::clamp(p.gate, 0.05f, 1.f);
    if(!scheduled || unit <= 0.)
    {
      out[0] = {0., scheduled ? gate : 1.};
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
      out[k] = {on, on + gate * (next - on)};
    }
    return n;
  }

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

  //! Plays step `step` of lane `i` at musical position `at`. When `scheduled`
  //! is false everything goes out at once, at `date`.
  void play_step(
      const Pattern& pat, int i, int step, int64_t pass, double at, double unit,
      bool scheduled, int64_t date) noexcept
  {
    const Lane& l = pat.lanes[i];
    auto& ls = lanes[i];
    const StepParams& p = stepParams(l, step);

    Note kind = audibleNote(pat, l, step, anySolo(pat));
    if(kind != Note::Rest)
    {
      if(!condition_holds(p, pass))
        kind = Note::Rest;
      else if(const float chance = p.chance * density; chance < 1.f)
        if(std::uniform_real_distribution<float>{0.f, 1.f}(rng) >= chance)
          kind = Note::Rest;
    }

    const auto push = [&](double position, event_kind k, int note, int velocity,
                          float value = 0.f) {
      push_event(
          {position, k, uint8_t(note), uint8_t(velocity), value}, scheduled, date);
    };

    if(l.note == 255 || l.note == 254)
    {
      push(
          at, l.note == 255 ? event_kind::accent : event_kind::slide, 0, 0,
          kind != Note::Rest ? 1.f : 0.f);
      return;
    }
    if(l.note > 127)
      return;

    int pitch = p.pitch;
    if(p.pitchRandom > 0 && kind != Note::Rest)
      pitch += std::uniform_int_distribution<int>{-p.pitchRandom, p.pitchRandom}(rng);
    float vel = p.velocity;
    if(p.velocityRandom > 0.f && kind != Note::Rest)
      vel += std::uniform_real_distribution<float>{
          -p.velocityRandom, p.velocityRandom}(rng);
    const int note = std::clamp(int(l.note) + pitch, 0, 127);
    const int velocity = std::clamp(
        int(std::lround(
            std::clamp(vel, 0.f, 1.f) * l.velocity * velocity_scale * 127.f)),
        1, 127);

    switch(kind)
    {
      case Note::Rest:
        if(ls.held >= 0)
          push(at, event_kind::note_off, ls.held, 0);
        ls.held = -1;
        break;

      case Note::Legato: {
        // The first hit ties on from the step before; a ratchet strikes the
        // others, each released after its gate, and holds the last into the
        // next step.
        if(ls.held != note)
        {
          if(ls.held >= 0)
            push(at, event_kind::note_off, ls.held, 0);
          push(at, event_kind::note_on, note, velocity);
          ls.held = note;
        }
        std::array<hit, max_ratchet> h;
        const int n = hits(p, unit, scheduled, h);
        for(int k = 1; k < n; k++)
        {
          push(at + h[k - 1].off * unit, event_kind::note_off, note, 0);
          push(at + h[k].on * unit, event_kind::note_on, note, velocity);
        }
        break;
      }

      case Note::Note: {
        if(ls.held >= 0)
          push(at, event_kind::note_off, ls.held, 0);
        ls.held = -1;

        std::array<hit, max_ratchet> h;
        const int n = hits(p, unit, scheduled, h);
        for(int k = 0; k < n; k++)
        {
          push(at + h[k].on * unit, event_kind::note_on, note, velocity);
          // The last hit with a full gate holds until the next step.
          if(k + 1 == n && h[k].off >= 1.)
            ls.held = note;
          else
            push(at + h[k].off * unit, event_kind::note_off, note, 0);
        }
        break;
      }
    }
  }

  void push_event(const event& e, bool scheduled, int64_t date) noexcept
  {
    if(!scheduled)
    {
      if(immediate_size < int(immediate.size()))
        immediate[immediate_size++] = e;
      return;
    }
    if(queue_size < max_events)
      queue[queue_size++] = e;
  }

  //! Every lane's releases go before any lane strikes.
  void flush_immediate(int64_t date) noexcept
  {
    std::stable_sort(
        immediate.begin(), immediate.begin() + immediate_size,
        [](const event& a, const event& b) { return a.kind < b.kind; });
    for(int i = 0; i < immediate_size; i++)
      emit(immediate[i], date);
    immediate_size = 0;
  }

  //! Sends what is due before musical position `end`, in order: at the same
  //! position a note-off goes before a note-on, so that a note restarted on
  //! the step that ends it is heard.
  void emit_until(
      const ossia::token_request& tk, int64_t tick_start, double samplesratio,
      double end) noexcept
  {
    const auto due_end = std::stable_partition(
        queue.begin(), queue.begin() + queue_size,
        [end](const event& e) { return e.position < end - 1e-9; });
    std::stable_sort(queue.begin(), due_end, [](const event& a, const event& b) {
      if(a.position != b.position)
        return a.position < b.position;
      return a.kind < b.kind;
    });

    for(auto it = queue.begin(); it != due_end; ++it)
      emit(*it, tick_start + tk.physical_position(it->position, samplesratio));

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
        if(in_flight.contains(e.note))
        {
          mess.push_back(libremidi::from_midi1::note_off(in_flight_channel, e.note, 0));
          mess.back().timestamp = date;
          in_flight.erase(e.note);
        }
        break;
      case event_kind::note_on:
        mess.push_back(libremidi::from_midi1::note_on(channel, e.note, e.velocity));
        mess.back().timestamp = date;
        in_flight.insert(e.note);
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
};
}
