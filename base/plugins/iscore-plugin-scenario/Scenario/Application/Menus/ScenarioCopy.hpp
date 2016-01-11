#pragma once
class QJsonObject;
class JSONObject;



namespace Scenario
{
class ConstraintModel;
class ScenarioModel;

class BaseScenario;
QJsonObject copyBaseConstraint(const ConstraintModel&);

QJsonObject copySelectedScenarioElements(const Scenario::ScenarioModel& sm);
QJsonObject copySelectedScenarioElements(BaseScenario& sm);
}
