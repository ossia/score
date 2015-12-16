#pragma once
class QJsonObject;
class JSONObject;



class ConstraintModel;
namespace Scenario {
class ScenarioModel;

}  // namespace Scenario

class BaseScenario;
QJsonObject copyBaseConstraint(const ConstraintModel&);

QJsonObject copySelectedScenarioElements(const Scenario::ScenarioModel& sm);
QJsonObject copySelectedScenarioElements(BaseScenario& sm);
