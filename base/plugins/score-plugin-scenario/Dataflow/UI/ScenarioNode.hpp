#pragma once

#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <Process/Process.hpp>
#include <score/model/Component.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
#include <score/model/ComponentFactory.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
namespace Dataflow
{

// For each Interval list the max num. of channels
// and create volume sliders and mix them
// For each Interval mix all the messages
// For each Interval mix all the midi
// For each state mix it.
// -> we have to add a link to the ossia::scenario
// -> we have to add data in events to indicate what states are to be executed
class Interval;
class ScenarioBase :
    public ProcessComponent_T<Scenario::ProcessModel>
{
    COMPONENT_METADATA("3ae53c95-fa10-47fa-a04f-eb766e3095f7")

public:
    ScenarioBase(
            Scenario::ProcessModel& scenario,
            DocumentPlugin& doc,
            const Id<score::Component>& id,
            QObject* parent_obj);

    struct IntervalData
    {
        Interval* component{};
        Slider* mix{};
    };

    Interval* make(
            const Id<score::Component>& id,
            Scenario::IntervalModel& elt);

    void setupInterval(Scenario::IntervalModel& c, Interval* comp);
    void teardownInterval(const Scenario::IntervalModel& c, const Interval& comp);

    template<typename... Args>
    bool removing(Args&&... args) { teardownInterval(std::forward<Args>(args)...); return true; }
    template<typename... Args>
    void removed(Args&&...) { }


private:
    NodeItem* ui{};
    NodeItem* slider{};
    SliderUI* sliderUI{};
};

using ScenarioComponent = SimpleHierarchicalScenarioComponent<
ScenarioBase,
Scenario::ProcessModel,
Interval>;

using ScenarioComponentFactory = ProcessComponentFactory_T<ScenarioComponent>;

}
