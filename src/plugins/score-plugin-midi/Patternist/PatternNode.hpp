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
#include <atomic>
#include <cmath>
#include <limits>

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

  int current = 0;
  //! The step played last, read by the interface.
  std::atomic_int last{-1};
  uint8_t channel{1};
  // Channel the notes currently in flight were started on: a channel change
  // must not leave them stranded on the previous one.
  uint8_t in_flight_channel{1};
  bool release_pending{};
  bool mustStop{};

  pattern_node()
  {
    in_flight.reserve(32);
    m_inlets.push_back(&pattern_select);
    m_inlets.push_back(&switch_quantification);
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

  bool legato(int note) const noexcept
  {
    for(const Lane& lane : pattern().lanes)
      if(lane.note == note && ossia::valid_index(current, lane.pattern))
        return lane.pattern[current] == Note::Legato;
    return false;
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
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    using namespace ossia;

    // Before every early return below: the inlets are cleared once the node has
    // run, so a request read later than this is a request lost.
    read_controls();

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

    constexpr auto never = std::numeric_limits<int64_t>::max();
    int64_t switch_at = -1;
    if(pending_pattern >= 0)
    {
      const auto points = tk.get_quantification_dates(switch_rate);
      // Points come in tick order in both directions, so the first one is the
      // earliest the switch can happen in this tick.
      if(!points.empty())
        switch_at = tk.physical_position(points.front().position, samplesratio);
    }

    // The two patterns can have different divisions, so the steps around the
    // switch are walked on their own grids rather than on a single one.
    play_steps(
        tk, tick_start, samplesratio, rewinding, 0, switch_at < 0 ? never : switch_at);

    if(switch_at >= 0)
    {
      apply_switch(tick_start + switch_at);
      play_steps(tk, tick_start, samplesratio, rewinding, switch_at, never);
    }
  }

  //! Plays the steps of the current pattern whose offset in the tick falls in
  //! [from_offset; to_offset[.
  void play_steps(
      const ossia::token_request& tk, int64_t tick_start, double samplesratio,
      bool rewinding, int64_t from_offset, int64_t to_offset) noexcept
  {
    const Pattern& pat = pattern();
    if(pat.length <= 0)
      return;
    if(current < 0 || current >= pat.length)
      current = 0;

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
      play_step(tick_start + offset, rewinding);
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
  }

  void play_step(int64_t date, bool rewinding = false) noexcept
  {
    const Pattern& pat = pattern();

    // Forward, the step about to play is the current one and the next tick
    // plays the one after. Rewinding mirrors that: step back first, so going
    // out and back over the same ground crosses the same steps.
    if(rewinding)
      current = (current + pat.length - 1) % pat.length;
    last.store(current, std::memory_order_relaxed);
    auto& mess = out.target<ossia::midi_port>()->messages;

    for(auto it = in_flight.begin(); it != in_flight.end();)
    {
      uint8_t note = *it;
      if(!legato(note))
      {
        mess.push_back(libremidi::from_midi1::note_off(in_flight_channel, note, 0));
        mess.back().timestamp = date;
        it = in_flight.erase(it);
      }
      else
      {
        ++it;
      }
    }

    in_flight_channel = channel;

    for(const Lane& lane : pat.lanes)
    {
      if(lane.note <= 127 && ossia::valid_index(current, lane.pattern))
      {
        switch(lane.pattern[current])
        {
          case Note::Note:
            mess.push_back(libremidi::from_midi1::note_on(channel, lane.note, 100));
            mess.back().timestamp = date;
            in_flight.insert(lane.note);
            break;
          case Note::Legato:
            if(!in_flight.contains(lane.note))
            {
              mess.push_back(libremidi::from_midi1::note_on(channel, lane.note, 100));
              mess.back().timestamp = date;
              in_flight.insert(lane.note);
            }
            break;
          case Note::Rest:
            if(in_flight.contains(lane.note))
            {
              mess.push_back(
                  libremidi::from_midi1::note_off(in_flight_channel, lane.note, 0));
              mess.back().timestamp = date;
              in_flight.erase(lane.note);
            }
            break;
        }
      }
    }

    for(const Lane& lane : pat.lanes)
    {
      if(ossia::valid_index(current, lane.pattern))
      {
        if(lane.note == 255)
        {
          if(lane.pattern[current] != Note::Rest)
            accent_out->write_value(1., date);
          else
            accent_out->write_value(0., date);
        }
        else if(lane.note == 254)
        {
          if(lane.pattern[current] != Note::Rest)
            slide_out->write_value(1., date);
          else
            slide_out->write_value(0., date);
        }
      }
    }

    if(!rewinding)
      current = (current + 1) % pat.length;
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
