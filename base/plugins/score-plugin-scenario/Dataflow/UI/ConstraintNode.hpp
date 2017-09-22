#pragma once
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <score_plugin_scenario_export.h>
#include <ossia/dataflow/graph_node.hpp>
namespace Dataflow
{
class Slider;
class interval_node : public ossia::graph_node
{
public:
  interval_node()
  {
    // todo maybe we can optimize by having m_outlets == m_inlets
    // this way no copy.
    m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
    m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());

    m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
    m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
  }

  void run(ossia::execution_state&) override
  {
    {
      auto i = m_inlets[0]->data.target<ossia::audio_port>();
      auto o = m_outlets[0]->data.target<ossia::audio_port>();
      o->samples = std::move(i->samples);
    }

    {
      auto i = m_inlets[1]->data.target<ossia::value_port>();
      auto o = m_outlets[1]->data.target<ossia::value_port>();
      o->data = std::move(i->data);
    }

    {
      auto i = m_inlets[1]->data.target<ossia::midi_port>();
      auto o = m_outlets[1]->data.target<ossia::midi_port>();
      o->messages = std::move(i->messages);
    }
  }
};


class IntervalBase :
        public Scenario::GenericIntervalComponent<Dataflow::DocumentPlugin>
{
    COMMON_COMPONENT_METADATA("eab98b28-5b0f-4754-aa3a-8d3622eedeea")
public:
    using parent_t = Scenario::GenericIntervalComponent<Dataflow::DocumentPlugin>;
    using DocumentPlugin = Dataflow::DocumentPlugin;
    using model_t = Process::ProcessModel;
    using component_t = Dataflow::ProcessComponent;
    using component_factory_list_t = Dataflow::ProcessComponentFactoryList;

    IntervalBase(
            const Id<score::Component>& id,
            Scenario::IntervalModel& interval,
            DocumentPlugin& doc,
            QObject* parent_comp);

    ProcessComponent* make(
            const Id<score::Component> & id,
            ProcessComponentFactory& factory,
            Process::ProcessModel &process);

    bool removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

    template <typename... Args>
    void removed(Args&&...)
    {
    }

private:
    NodeItem* ui{};
    NodeItem* slider{};
    SliderUI* sliderUI{};
};

class Interval final :
        public score::PolymorphicComponentHierarchy<IntervalBase>
{
public:
    using score::PolymorphicComponentHierarchy<IntervalBase>::PolymorphicComponentHierarchyManager;
};

}
