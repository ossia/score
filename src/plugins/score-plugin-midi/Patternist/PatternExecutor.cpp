// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/port.hpp>

#include <QTimer>

#include <Patternist/PatternModel.hpp>
#include <libremidi/ump_events.hpp>

#include <algorithm>
#include <cmath>

namespace Patternist
{
// The model stores channels as 1-16, the wire format as 0-15
static uint8_t to_midi_channel(int c) noexcept
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
    if(tk.model_read_duration() == 0_tv)
      return;

    const double samplesratio = st.modelToSamples();
    const double speed = tk.speed != 0. ? tk.speed : 1.;
    const int64_t tick_start = std::floor(tk.offset.impl * samplesratio / speed);

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
    if(auto d = tk.get_quantification_date(pattern.division))
    {
      const int64_t date
          = std::floor((*d - tk.prev_date + tk.offset).impl * samplesratio / speed);

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

      current = (current + 1) % pattern.length;
    }
  }

  void all_notes_off() noexcept override { release_all(0); }
};

Executor::Executor(
    Patternist::ProcessModel& element, const Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<Patternist::ProcessModel, ossia::node_process>{
        element, ctx, "PatternComponent", parent}
{
  auto node = ossia::make_node<pattern_node>(*ctx.execState);
  node->channel = to_midi_channel(element.channel());
  node->in_flight_channel = node->channel;
  node->pattern = element.patterns()[element.currentPattern()];
  node->current = 0;

  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element, &Patternist::ProcessModel::channelChanged, this, [this, node](int c) {
    in_exec([node, c = to_midi_channel(c)] { node->set_channel(c); });
  });
  con(element, &Patternist::ProcessModel::currentPatternChanged, this,
      [this, node, &element](int c) {
    in_exec([node, p = element.patterns()[c]] { node->pattern = p; });
  });
  con(element, &Patternist::ProcessModel::patternsChanged, this,
      [this, node, &element]() {
    in_exec(
        [node, p = element.patterns()[element.currentPattern()]] { node->pattern = p; });
  });
  con(ctx.doc.execTimer, &QTimer::timeout, this, [&element, node] {
    int c = node->last;
    element.execPosition(c);
  });
}

void Executor::stop()
{
  ProcessComponent::stop();
  this->process().execPosition(-1);
}
Executor::~Executor() { }
}
