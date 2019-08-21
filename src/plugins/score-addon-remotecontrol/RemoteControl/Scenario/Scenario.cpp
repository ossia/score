#include "Scenario.hpp"
namespace RemoteControl
{

ScenarioBase::ScenarioBase(
    Scenario::ProcessModel& scenario,
    DocumentPlugin& doc,
    const Id<score::Component>& id,
    QObject* parent_obj)
    : ProcessComponent_T<::Scenario::ProcessModel>{scenario,
                                                   doc,
                                                   id,
                                                   "ScenarioComponent",
                                                   parent_obj}
{
}

}
