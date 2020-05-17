#pragma once
#include <Midi/MidiNote.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/editor/scenario/time_process.hpp>
namespace ossia::nodes
{
struct note_data;
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
class Component final
    : public ::Execution::ProcessComponent_T<Midi::ProcessModel, ossia::node_process>,
      public Nano::Observer
{
  COMPONENT_METADATA("6d5334a5-7b8c-45df-9805-11d1b4472cdf")
public:
  static const constexpr bool is_unique = true;
  Component(
      Midi::ProcessModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Component() override;

private:
  void on_noteAdded(const Midi::Note&);
  void on_noteRemoved(const Midi::Note&);

  ossia::nodes::note_data to_note(const NoteData& n);
};

using ComponentFactory = ::Execution::ProcessComponentFactory_T<Component>;
}
}
