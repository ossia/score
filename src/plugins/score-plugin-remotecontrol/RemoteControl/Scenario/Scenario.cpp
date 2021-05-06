#include "Scenario.hpp"
namespace RemoteControl
{

ScenarioBase::ScenarioBase(
    Scenario::ProcessModel& scenario,
    DocumentPlugin& doc,
    QObject* parent_obj)
    : ProcessComponent_T<::Scenario::ProcessModel>{
        scenario,
        doc,
        "ScenarioComponent",
        parent_obj}
{
}

}
