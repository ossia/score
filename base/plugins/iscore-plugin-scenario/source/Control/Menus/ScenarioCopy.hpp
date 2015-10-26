#pragma once
#include <QJsonObject>
class ConstraintModel;
class ScenarioModel;

QJsonObject copyBaseConstraint(const ConstraintModel&);
QJsonObject copySelectedScenarioElements(const ScenarioModel&);
