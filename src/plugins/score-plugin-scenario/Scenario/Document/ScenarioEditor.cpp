#include "ScenarioEditor.hpp"
#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
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

static bool pasteInScenario(QPoint pos, ScenarioPresenter& pres, const QMimeData& mime, const score::DocumentContext& ctx)
{
  auto& sm = static_cast<const Scenario::ProcessModel&>(pres.model());
  ScenarioView& sv = pres.view();

  auto sv_pt = mapPointToItem(pos, sv);
  if(!sv_pt)
    return false;

  // TODO this is a bit lazy.. find a better positoning algorithm
  if (!sv.contains(*sv_pt))
    sv_pt = sv.mapToScene(sv.boundingRect().center());

  // Read the copy json. TODO: give it a better mime type
  auto origin = pres.toScenarioPoint(*sv_pt);
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

static bool pasteInCurrentInterval(QPoint pos, const QMimeData& mime, const score::DocumentContext& ctx)
{
  auto pres = score::IDocument::presenterDelegate<ScenarioDocumentPresenter>(ctx.document);
  if(!pres)
    return false;

  // Get the QGraphicsView
  auto view = &pres->view().view();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(pos);
  auto scene_pt = view->mapToScene(view_pt);

  // We may have to paste in the nodal view or in the temporal view,
  // with different outcomes
  struct NodalPositionVisitor {
      QPointF& p;
      QPointF operator()(const NodalIntervalView& nodal) const
      {
        auto pt = nodal.nodeContainer().mapFromScene(p);
        // TODO clamp to the visible rect... it isn't boundingRect().
        // QRectF nodalRect = nodal.boundingRect();
        // if(!nodalRect.contains(pt))
        //   return QPointF{};
        return pt;
      }

      QPointF operator()(const CentralIntervalDisplay& disp) const
      {
        auto itv_pres = disp.presenter.intervalPresenter();
        const auto& slots = itv_pres->getSlots();
        auto nodal_it = ossia::find_if(
                          slots, []
                          (const SlotPresenter& slot) { return bool(slot.getNodalSlot()); });
        if(nodal_it == slots.end())
          return QPointF{};

        auto nodal = nodal_it->getNodalSlot();
        if(nodal && nodal->view)
          return (*this)(*nodal->view);

        return QPointF{};
      }

      QPointF operator()(const CentralNodalDisplay& disp) const {
        return (*this)(*disp.presenter);
      }

      QPointF operator()(std::monostate) const { return QPointF{}; }
  };

  auto item_pt = std::visit(NodalPositionVisitor{scene_pt}, pres->display());

  auto& itv = pres->displayedInterval();
  auto obj = readJson(mime.data("text/plain"));
  auto proc_it = obj.FindMember("Processes");
  if(proc_it == obj.MemberEnd() || !proc_it->value.IsArray())
    return false;

  auto cables_it = obj.FindMember("Cables");
  if(cables_it == obj.MemberEnd() || !cables_it->value.IsArray())
    return false;

  {
    auto processes = proc_it->value.GetArray();
    if(processes.Empty())
      return true;

    auto cables = cables_it->value.GetArray();

    auto cmd = new Scenario::Command::PasteProcessesInInterval{processes, cables, itv, ExpandMode{}, item_pt};
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);


    // FIXME paste all cables recursively ! e.g. check copy-pasting a scenario
    return true;
  }
  return false;
}

bool ScenarioEditor::paste(QPoint pos, QObject* focusedObject, const QMimeData& mime, const score::DocumentContext& ctx)
{
  // Check if we are focusing a scenario in which to paste
  if(auto pres = qobject_cast<ScenarioPresenter*>(focusedObject))
  {
    return pasteInScenario(pos, *pres, mime, ctx);
  }
  else
  {
    return pasteInCurrentInterval(pos, mime, ctx);
  }
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
