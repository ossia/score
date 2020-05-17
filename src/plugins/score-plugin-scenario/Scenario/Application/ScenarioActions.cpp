// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioActions.hpp"

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <core/document/DocumentModel.hpp>

namespace Scenario
{

const ScenarioInterface* focusedScenarioInterface(const score::DocumentContext& ctx)
{
  if (auto layer
      = dynamic_cast<const Scenario::ScenarioInterface*>(ctx.document.focusManager().get()))
  {
    return layer;
  }
  else
  {
    auto model
        = dynamic_cast<Scenario::ScenarioDocumentModel*>(&ctx.document.model().modelDelegate());
    if (model)
    {
      auto& bs = model->baseScenario();
      if (bs.focused())
      {
        return &bs;
      }
    }
  }
  return nullptr;
}

const ProcessModel* focusedScenarioModel(const score::DocumentContext& ctx)
{
  return dynamic_cast<const Scenario::ProcessModel*>(ctx.document.focusManager().get());
}

EnableWhenScenarioModelObject::EnableWhenScenarioModelObject()
    : score::ActionCondition{static_key()}
{
}

score::ActionConditionKey EnableWhenScenarioModelObject::static_key()
{
  return score::ActionConditionKey{"ScenarioModelObject"};
}

void EnableWhenScenarioModelObject::action(score::ActionManager& mgr, score::MaybeDocument doc)
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
    return bool(dynamic_cast<const Scenario::IntervalModel*>(ptr))
           || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
           || bool(dynamic_cast<const Scenario::StateModel*>(ptr))
           || bool(dynamic_cast<const Scenario::CommentBlockModel*>(ptr))
           || bool(dynamic_cast<const Scenario::TimeSyncModel*>(ptr))
           || bool(dynamic_cast<const Process::ProcessModel*>(ptr));
  });

  setEnabled(mgr, res);
}

EnableWhenScenarioInterfaceInstantObject::EnableWhenScenarioInterfaceInstantObject()
    : score::ActionCondition{static_key()}
{
}

score::ActionConditionKey EnableWhenScenarioInterfaceInstantObject::static_key()
{
  return score::ActionConditionKey{"ScenarioInterfaceInstantObject"};
}

void EnableWhenScenarioInterfaceInstantObject::action(
    score::ActionManager& mgr,
    score::MaybeDocument doc)
{
  if (!doc)
  {
    setEnabled(mgr, false);
    return;
  }

  const auto& sel = doc->selectionStack.currentSelection();
  auto res = ossia::any_of(sel, [](auto obj) {
    auto ptr = obj.data();
    return bool(dynamic_cast<const Scenario::TimeSyncModel*>(ptr))
           || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
           || bool(dynamic_cast<const Scenario::StateModel*>(ptr));
  });

  setEnabled(mgr, res);
}

EnableWhenScenarioInterfaceObject::EnableWhenScenarioInterfaceObject()
    : score::ActionCondition{static_key()}
{
}

score::ActionConditionKey EnableWhenScenarioInterfaceObject::static_key()
{
  return score::ActionConditionKey{"ScenarioInterfaceObject"};
}

void EnableWhenScenarioInterfaceObject::action(score::ActionManager& mgr, score::MaybeDocument doc)
{
  if (!doc)
  {
    setEnabled(mgr, false);
    return;
  }

  const auto& sel = doc->selectionStack.currentSelection();
  auto res = ossia::any_of(sel, [](auto obj) {
    auto ptr = obj.data();
    return bool(dynamic_cast<const Scenario::IntervalModel*>(ptr))
           || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
           || bool(dynamic_cast<const Scenario::StateModel*>(ptr));
  }); // TODO why not timesync

  setEnabled(mgr, res);
}
}
