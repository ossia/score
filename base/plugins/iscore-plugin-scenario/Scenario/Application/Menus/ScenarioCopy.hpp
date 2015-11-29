#pragma once
#include <qjsonobject.h>

class ConstraintModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

QJsonObject copyBaseConstraint(const ConstraintModel&);
QJsonObject copySelectedScenarioElements(const Scenario::ScenarioModel&);
