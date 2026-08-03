#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/math.hpp>

#include <Patternist/PatternModel.hpp>
#include <libremidi/ump_events.hpp>

#include <algorithm>
#include <cmath>

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
  ossia::midi_outlet out;
  ossia::value_outlet accent_out;
  ossia::value_outlet slide_out;
  Pattern pattern;
  ossia::flat_set<uint8_t> in_flight;

  int current = 0;
  int last = -1;
  uint8_t channel{1};
  // Channel the notes currently in flight were started on: a channel change
  // must not leave them stranded on the previous one.
  uint8_t in_flight_channel{1};
  bool release_pending{};
  bool mustStop{};

  pattern_node()
  {
    in_flight.reserve(32);
    m_outlets.push_back(&out);
    m_outlets.push_back(&accent_out);
    m_outlets.push_back(&slide_out);
  }

  std::string label() const noexcept override { return "pattern_node"; }

  bool legato(int note) const noexcept
  {
    for(const Lane& lane : pattern.lanes)
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

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    using namespace ossia;

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

    if(pattern.length <= 0)
      return;

    // TODO on bar change, reset to start of pattern?
    // All of them, not just the first: a tick covers more than one step as soon
    // as the division is small, the buffer large or the tempo high, and the
    // single-date version would silently drop every step but one.
    // get_quantification_dates walks the grid in tick order, so rewinding it
    // hands back the steps the tick crosses in decreasing musical order. Walk
    // the pattern the same way, or the sequence marches on while the timeline
    // runs the other way.
    const bool rewinding = tk.backward();
    for(const auto& q : tk.get_quantification_dates(pattern.division))
    {
      // Through the tick's one musical-position -> sample map, not through the
      // point's date: the date is truncated to a whole flick, so flooring it
      // into a sample rounds twice and puts the step a sample before the
      // metronome click on the same bar line.
      const int64_t date = tick_start + tk.physical_position(q.position, samplesratio);
      play_step(date, rewinding);
    }
  }

  void play_step(int64_t date, bool rewinding = false) noexcept
  {
    // Forward, the step about to play is the current one and the next tick
    // plays the one after. Rewinding mirrors that: step back first, so going
    // out and back over the same ground crosses the same steps.
    if(rewinding)
      current = (current + pattern.length - 1) % pattern.length;
    last = current;
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

    for(Lane& lane : pattern.lanes)
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

    for(Lane& lane : pattern.lanes)
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
      current = (current + 1) % pattern.length;
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
