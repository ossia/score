#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Midi/MidiNote.hpp>
#include <boost/container/flat_set.hpp>
#include <ossia/dataflow/node_process.hpp>
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
struct note_data;
class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<Midi::ProcessModel, ossia::node_process>
    , public Nano::Observer
{
  COMPONENT_METADATA("6d5334a5-7b8c-45df-9805-11d1b4472cdf")
public:
    static const constexpr bool is_unique = true;
  Component(
      Midi::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Component() override;

  private:
  void on_noteAdded(const Midi::Note&);
  void on_noteRemoved(const Midi::Note&);

  note_data to_note(const NoteData& n);

};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
