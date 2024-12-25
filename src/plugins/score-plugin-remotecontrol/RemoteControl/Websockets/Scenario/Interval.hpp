#pragma once
#include <Scenario/Document/Components/IntervalComponent.hpp>

#include <score/model/ComponentHierarchy.hpp>

#include <RemoteControl/Websockets/DocumentPlugin.hpp>
#include <RemoteControl/Websockets/Scenario/Process.hpp>

namespace RemoteControl::WS
{
class IntervalBase
    : public Scenario::GenericIntervalComponent<RemoteControl::WS::DocumentPlugin>
{
  COMMON_COMPONENT_METADATA("b079041c-f11f-49b1-a88f-b2bc070affb1")
public:
  using parent_t = Scenario::GenericIntervalComponent<RemoteControl::WS::DocumentPlugin>;
  using DocumentPlugin = RemoteControl::WS::DocumentPlugin;
  using model_t = Process::ProcessModel;
  using component_t = RemoteControl::WS::ProcessComponent;
  using component_factory_list_t = RemoteControl::WS::ProcessComponentFactoryList;

  IntervalBase(
      Scenario::IntervalModel& Interval, DocumentPlugin& doc, QObject* parent_comp);

  ~IntervalBase();

  ProcessComponent*
  make(ProcessComponentFactory& factory, Process::ProcessModel& process);
  ProcessComponent* make(Process::ProcessModel& process);

  bool removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

  template <typename... Args>
  void added(Args&&...)
  {
  }
  template <typename... Args>
  void removed(Args&&...)
  {
  }
};

class Interval final : public score::PolymorphicComponentHierarchy<IntervalBase>
{
public:
  using score::PolymorphicComponentHierarchy<
      IntervalBase>::PolymorphicComponentHierarchyManager;
};
}
