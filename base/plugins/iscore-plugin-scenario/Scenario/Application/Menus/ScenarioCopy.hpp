#pragma once
class QJsonObject;
class QObject;


namespace Scenario
{
class ConstraintModel;
class ScenarioModel;

class BaseScenario;
class BaseScenarioContainer;
QJsonObject copyBaseConstraint(const ConstraintModel&);

QJsonObject copySelectedScenarioElements(const Scenario::ScenarioModel& sm);

/**
 * The parent should be in the object tree of the scenario.
 * This is because the StateModel needs acces to the command stack
 * of the document upon creation.
 *
 * TODO instead we should follow the second
 * part of this article : https://doc.qt.io/archives/qq/qq25-undo.html
 * which explains how to use a proxy model to perform the undo - redo operations.
 * This proxy model should be owned by the presenters where there is an easy and
 * sensical access to the command stack
 */
//QJsonObject copySelectedScenarioElements(
//        const BaseScenario& sm,
//        QObject* parent);
QJsonObject copySelectedScenarioElements(
        const BaseScenarioContainer& sm,
        QObject* parent);
}
