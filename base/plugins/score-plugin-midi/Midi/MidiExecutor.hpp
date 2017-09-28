#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Midi/MidiNote.hpp>
#include <boost/container/flat_set.hpp>
namespace ossia
{
namespace net
{
namespace midi
{
class channel_node;
}
}
}

namespace Device
{
class DeviceList;
}
namespace Midi
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final : public ossia::time_process
{
public:
  ProcessExecutor(
      const Midi::ProcessModel& proc, const Device::DeviceList& devices);
  ~ProcessExecutor();

  const Midi::ProcessModel& process() const { return m_process; }

  ossia::state_element state(ossia::time_value date, double pos, ossia::time_value tick_offset) override;
  ossia::state_element offset(ossia::time_value, double pos) override;

  void stop() override;

  struct TimedState {
    std::vector<std::pair<NoteData, uint32_t>> currentAudioStart;
    std::vector<std::pair<NoteData, uint32_t>> currentAudioStop;
  };
  TimedState timedState;

private:
  const Midi::ProcessModel& m_process;

  ossia::net::midi::channel_node* m_channelNode{};
  ossia::state m_lastState;

  using note_set = boost::container::flat_multiset<NoteData, NoteComparator>;
  note_set m_notes;
  note_set m_playingnotes;

  boost::container::flat_set<int> m_playing;
};

class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<Midi::ProcessModel, ProcessExecutor>
{
  COMPONENT_METADATA("6d5334a5-7b8c-45df-9805-11d1b4472cdf")
public:
    static const constexpr bool is_unique = true;
  Component(
      Engine::Execution::IntervalComponent& parentInterval,
      Midi::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Component() override;

  private:
  ossia::node_ptr m_node;
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
