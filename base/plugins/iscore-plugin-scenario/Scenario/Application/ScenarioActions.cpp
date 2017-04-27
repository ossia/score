#include "ScenarioActions.hpp"
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <core/document/DocumentModel.hpp>

namespace Scenario
{

const ScenarioInterface*
focusedScenarioInterface(const iscore::DocumentContext& ctx)
{
  if (auto layer = focusedScenarioModel(ctx))
  {
    return layer;
  }
  else
  {
    auto model = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &ctx.document.model().modelDelegate());
    auto& bs = model->baseScenario();
    if (model && bs.focused())
    {
      return &bs;
    }
  }
  return nullptr;
}

const ProcessModel* focusedScenarioModel(const iscore::DocumentContext& ctx)
{
  return dynamic_cast<const Scenario::ProcessModel*>(ctx.document.focusManager().get());
}

EnableWhenScenarioModelObject::EnableWhenScenarioModelObject()
    : iscore::ActionCondition{static_key()}
{
}

iscore::ActionConditionKey EnableWhenScenarioModelObject::static_key()
{
  return iscore::ActionConditionKey{"ScenarioModelObject"};
}

void EnableWhenScenarioModelObject::action(
    iscore::ActionManager& mgr, iscore::MaybeDocument doc)
{
  if (!doc)
  {
    setEnabled(mgr, false);
    return;
  }

  auto focus = doc->focus.get();
  if (!focus)
  {
    setEnabled(mgr, false);
    return;
  }

  auto proc = dynamic_cast<const Scenario::ProcessModel*>(focus);
  if (!proc)
  {
    setEnabled(mgr, false);
    return;
  }

  const auto& sel = doc->selectionStack.currentSelection();
  auto res = ossia::any_of(sel, [](auto obj) {
    auto ptr = obj.data();
    return bool(dynamic_cast<const Scenario::ConstraintModel*>(ptr))
           || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
           || bool(dynamic_cast<const Scenario::StateModel*>(ptr))
           || bool(dynamic_cast<const Scenario::CommentBlockModel*>(ptr));
  }); // TODO why not timenode

  setEnabled(mgr, res);
}



EnableWhenScenarioInterfaceInstantObject::EnableWhenScenarioInterfaceInstantObject()
    : iscore::ActionCondition{static_key()}
{
}

iscore::ActionConditionKey EnableWhenScenarioInterfaceInstantObject::static_key()
{
  return iscore::ActionConditionKey{"ScenarioInterfaceInstantObject"};
}

void EnableWhenScenarioInterfaceInstantObject::action(
    iscore::ActionManager& mgr, iscore::MaybeDocument doc)
{
  if (!doc)
  {
    setEnabled(mgr, false);
    return;
  }

  const auto& sel = doc->selectionStack.currentSelection();
  auto res = ossia::any_of(sel, [](auto obj) {
    auto ptr = obj.data();
    return bool(dynamic_cast<const Scenario::TimeNodeModel*>(ptr))
           || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
           || bool(dynamic_cast<const Scenario::StateModel*>(ptr));
  });

  setEnabled(mgr, res);
}



EnableWhenScenarioInterfaceObject::EnableWhenScenarioInterfaceObject()
    : iscore::ActionCondition{static_key()}
{
}

iscore::ActionConditionKey EnableWhenScenarioInterfaceObject::static_key()
{
  return iscore::ActionConditionKey{"ScenarioInterfaceObject"};
}

void EnableWhenScenarioInterfaceObject::action(
    iscore::ActionManager& mgr, iscore::MaybeDocument doc)
{
  if (!doc)
  {
    setEnabled(mgr, false);
    return;
  }

  const auto& sel = doc->selectionStack.currentSelection();
  auto res = ossia::any_of(sel, [](auto obj) {
    auto ptr = obj.data();
    return bool(dynamic_cast<const Scenario::ConstraintModel*>(ptr))
           || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
           || bool(dynamic_cast<const Scenario::StateModel*>(ptr));
  }); // TODO why not timenode

  setEnabled(mgr, res);
}
}
