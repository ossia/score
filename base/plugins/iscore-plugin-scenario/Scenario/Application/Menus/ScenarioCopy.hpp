#pragma once
#include <QJsonObject>
class ConstraintModel;
namespace Scenario { class ScenarioModel; }

QJsonObject copyBaseConstraint(const ConstraintModel&);
QJsonObject copySelectedScenarioElements(const Scenario::ScenarioModel&);
