#pragma once
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <LocalTree/EventComponent.hpp>
#include <LocalTree/IntervalComponent.hpp>
#include <LocalTree/StateComponent.hpp>
#include <LocalTree/TimeSyncComponent.hpp>

namespace LocalTree
{
// Exposes a Sequence's internal structure (sections, ISes) in the document's
// local device tree, so external clients and mappings can observe and drive
// them like any scenario element.
class SequenceComponentBase : public ProcessComponent_T<Sequence::SequenceModel>
{
  COMPONENT_METADATA("12fc2ac2-a376-4146-b1cf-0f021c0f6d3d")

public:
  SequenceComponentBase(
      ossia::net::node_base& parent, Sequence::SequenceModel& sequence,
      const score::DocumentContext& doc, QObject* parent_obj);

  template <typename Component_T, typename Element>
  Component_T* make(Element& elt);

  template <typename... Args>
  bool removing(Args&&...)
  {
    return true;
  }
  template <typename... Args>
  void removed(Args&&...)
  {
  }

private:
  ossia::net::node_base& m_intervalsNode;
  ossia::net::node_base& m_eventsNode;
  ossia::net::node_base& m_timeSyncsNode;
  ossia::net::node_base& m_statesNode;
};

using SequenceComponent = HierarchicalScenarioComponent<
    SequenceComponentBase, Sequence::SequenceModel, Interval, Event, TimeSync, State>;

using SequenceComponentFactory = LocalTree::ProcessComponentFactory_T<SequenceComponent>;
}
