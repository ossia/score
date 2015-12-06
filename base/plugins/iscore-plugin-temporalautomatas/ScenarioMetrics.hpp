#pragma once
#include <QString>

namespace Scenario
{
class ScenarioModel;
namespace Metrics
{
int halstead(const ScenarioModel& scenar);

QString toScenarioLanguage(const Scenario::ScenarioModel& s);
}
}
