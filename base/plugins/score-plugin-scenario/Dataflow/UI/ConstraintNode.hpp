#pragma once
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <score_plugin_scenario_export.h>
#include <ossia/dataflow/graph_node.hpp>
namespace Dataflow
{
class Slider;
/*

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
*/
}
