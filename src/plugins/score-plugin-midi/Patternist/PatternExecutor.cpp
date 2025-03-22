// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/port.hpp>

#include <QTimer>

#include <Patternist/PatternModel.hpp>
#include <libremidi/ump_events.hpp>

namespace Patternist
{
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
      if(lane.note == note)
        return lane.pattern[current] == Note::Legato;
    return false;
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    using namespace ossia;
    if(tk.model_read_duration() == 0_tv)
      return;
    if(tk.end_discontinuous)
    {
      auto& mess = out.target<ossia::midi_port>()->messages;
      for(uint8_t note : in_flight)
      {
        mess.push_back(libremidi::from_midi1::note_off(channel, note, 0));
        mess.back().timestamp = 0;
      }
      in_flight.clear();
      return;
    }

    // TODO on bar change, reset to start of pattern?
    if(auto d = tk.get_quantification_date(pattern.division))
    {
      auto date = std::floor(
          (*d - tk.prev_date + tk.offset).impl * st.modelToSamples() / tk.speed);

      last = current;
      auto& mess = out.target<ossia::midi_port>()->messages;

      for(auto it = in_flight.begin(); it != in_flight.end();)
      {
        uint8_t note = *it;
        if(!legato(note))
        {
          mess.push_back(libremidi::from_midi1::note_off(channel, note, 0));
          mess.back().timestamp = date;
          it = in_flight.erase(it);
        }
        else
        {
          ++it;
        }
      }

      for(Lane& lane : pattern.lanes)
      {
        if(lane.note <= 127)
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
                mess.push_back(libremidi::from_midi1::note_off(channel, lane.note, 0));
                mess.back().timestamp = date;
                in_flight.erase(lane.note);
              }
              break;
          }
        }
      }

      for(Lane& lane : pattern.lanes)
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

      current = (current + 1) % pattern.length;
    }
  }

  void all_notes_off() noexcept override
  {
    auto& mess = out.target<ossia::midi_port>()->messages;
    for(uint8_t note : in_flight)
    {
      mess.push_back(libremidi::from_midi1::note_off(channel, note, 0));
      mess.back().timestamp = 0;
    }
  }
};

Executor::Executor(
    Patternist::ProcessModel& element, const Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<Patternist::ProcessModel, ossia::node_process>{
        element, ctx, "PatternComponent", parent}
{
  auto node = ossia::make_node<pattern_node>(*ctx.execState);
  node->channel = element.channel() - 1;
  node->pattern = element.patterns()[element.currentPattern()];
  node->current = 0;

  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element, &Patternist::ProcessModel::channelChanged, this,
      [this, node](int c) { in_exec([=] { node->channel = c; }); });
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
