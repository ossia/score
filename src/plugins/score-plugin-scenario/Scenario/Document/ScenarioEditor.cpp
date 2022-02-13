#include "ScenarioEditor.hpp"
#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>

#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QClipboard>

namespace Scenario
{

bool ScenarioEditor::copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx)
{
  if (auto si = focusedScenarioInterface(ctx))
  {
    Scenario::copySelectedElementsToJson(r, *const_cast<ScenarioInterface*>(si), ctx);
    return true;
  }
  else
  {
    return Scenario::copySelectedProcesses(r, ctx);
  }
  return false;
}

bool ScenarioEditor::paste(QPoint pos, const QMimeData& mime, const score::DocumentContext& ctx)
{
  // Check if we are focusing a scenario in which to paste
  auto focus = Process::ProcessFocusManager::get(ctx);
  if(!focus)
    return false;

  auto pres = qobject_cast<ScenarioPresenter*>(focus->focusedPresenter());
  if (!pres)
    return false;

  auto& sm = static_cast<const Scenario::ProcessModel&>(pres->model());

  // Get the QGraphicsView
  auto views = pres->view().scene()->views();
  if (views.empty())
    return false;

  auto view = views.front();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(pos);
  auto scene_pt = view->mapToScene(view_pt);
  ScenarioView& sv = pres->view();
  auto sv_pt = sv.mapFromScene(scene_pt);

  // TODO this is a bit lazy.. find a better positoning algorithm
  if (!sv.contains(sv_pt))
    sv_pt = sv.mapToScene(sv.boundingRect().center());

  // Read the copy json. TODO: give it a better mime type
  auto origin = pres->toScenarioPoint(sv_pt);
  auto obj = readJson(mime.data("text/plain"));

  if (!obj.IsObject() || obj.MemberCount() == 0)
    return false;
  if (!obj.HasMember("TimeNodes"))
    return false;

  // TODO check json validity
  // Submit the paste command
  auto cmd = new Command::ScenarioPasteElements(sm, obj, origin);
  CommandDispatcher<>{ctx.commandStack}.submit(cmd);
  return true;
}

bool ScenarioEditor::remove(const Selection& s, const score::DocumentContext& ctx)
{
  if (s.size() == 1)
  {
    CommandDispatcher<> d{ctx.commandStack};

    auto first = s.begin()->data();
    if (auto c = qobject_cast<const Process::Cable*>(first))
    {
      auto& doc = score::IDocument::get<ScenarioDocumentModel>(ctx.document);
      d.submit<Dataflow::RemoveCable>(doc, *c);
      return true;
    }
    else if (auto proc = qobject_cast<const Process::ProcessModel*>(first))
    {
      using namespace Command;
      auto p = proc->parent();
      if (auto itv = qobject_cast<IntervalModel*>(p))
      {
        d.submit<RemoveProcessFromInterval>(*itv, proc->id());
        return true;
      }
      else if (auto st = qobject_cast<StateModel*>(p))
      {
        d.submit<RemoveStateProcess>(*st, proc->id());
        return true;
      }
      else
      {
        return false;
      }
    }
  }

  if (auto sm = focusedScenarioModel(ctx))
  {
    if (s.size() == 1)
    {
      auto first = s.begin()->data();
      if (auto cb = qobject_cast<const Scenario::CommentBlockModel*>(first))
      {
        CommandDispatcher<> d{ctx.commandStack};
        d.submit<Command::RemoveCommentBlock>(*sm, *cb);
        return true;
      }
    }

    Scenario::removeSelection(*sm, ctx);
    return true;
  }

  auto si = focusedScenarioInterface(ctx);
  if (si)
  {
    Scenario::clearContentFromSelection(*si, ctx);
    return true;
  }

  return false;
}
}
