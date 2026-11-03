#include "ScenarioEditor.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Effect/EffectLayer.hpp>
#include <Scenario/Commands/Interval/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/Commands/Scenario/PasteAnchors.hpp>
#include <Scenario/Commands/Scenario/ScenarioPaste.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/SafeCast.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QClipboard>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <algorithm>

namespace Scenario
{

bool ScenarioEditor::copy(
    JSONReader& r, const Selection& s, const score::DocumentContext& ctx)
{
  if(auto si = focusedScenarioInterface(ctx))
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

std::vector<ObjectPath> loadCopiedProcesses(
    Scenario::Command::Macro& m, const Scenario::IntervalModel& interval,
    const rapidjson::Value& copy, std::vector<std::pair<int32_t, int32_t>>& proc_id_map,
    const score::DocumentContext& ctx)
{
  std::vector<ObjectPath> pasted;
  auto proc_it = copy.FindMember("Processes");
  if(proc_it == copy.MemberEnd() || !proc_it->value.IsArray())
    return pasted;
  const auto processes = proc_it->value.GetArray();

  if(hasAnchors(proc_it->value))
  {
    // The anchors are remapped on instances of the copies
    ProcessesBeingCopied copied{processes, interval, ctx};
    copied.remapAnchors(copy, interval, ctx);
    std::vector<Scenario::Command::DuplicateOnlyProcessToInterval*> loads;
    for(std::size_t i = 0; i < copied.processes.size(); i++)
    {
      auto proc = copied.processes[i];
      for(auto port : proc->findChildren<Process::Port*>())
        while(!port->cables().empty())
          port->removeCable(port->cables().back());

      loads.push_back(new Scenario::Command::DuplicateOnlyProcessToInterval{
          interval, copied.processes_ids[i], *proc});
      proc_id_map.emplace_back(proc->id().val(), copied.processes_ids[i].val());
      delete proc;
    }

    for(auto load : loads)
    {
      const auto id = load->processId();
      m.submit(load);
      auto& new_proc = interval.processes.at(id);
      m.createViewForNewProcess(interval, new_proc);
      pasted.push_back(score::IDocument::unsafe_path(new_proc));
    }
  }
  else
  {
    for(auto& proc : processes)
    {
      if(!proc.IsObject() || !proc.HasMember(score::StringConstant().uuid))
        continue;
      auto id_it = proc.FindMember("id");
      if(id_it == proc.MemberEnd() || !id_it->value.IsInt())
        continue;
      int32_t old_id = id_it->value.GetInt();
      if(auto* new_proc = m.loadProcessInSlot(interval, proc))
      {
        proc_id_map.emplace_back(old_id, new_proc->id().val());
        pasted.push_back(score::IDocument::unsafe_path(*new_proc));
      }
    }
  }
  return pasted;
}

void duplicateProcess(
    const IntervalModel& interval, const Process::ProcessModel& proc,
    const score::DocumentContext& ctx)
{
  JSONReader r;
  r.stream.StartObject();
  Process::copyProcesses(r, {&proc}, ctx);
  r.stream.EndObject();
  const auto copy = readJson(r.toByteArray());

  Scenario::Command::Macro m{new Scenario::Command::DuplicateProcess, ctx};
  std::vector<std::pair<int32_t, int32_t>> ids;
  auto pasted = loadCopiedProcesses(m, interval, copy, ids, ctx);
  if(pasted.empty())
    return;
  m.submit(new Scenario::Command::FollowPasted{std::move(pasted)});
  m.commit();
}

bool pasteProcessesInNewBox(
    const Scenario::ProcessModel& sm, Scenario::Point origin, rapidjson::Value& obj,
    const score::DocumentContext& ctx, bool startsOnPlay)
{
  // Create a box
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, ctx};

  auto proc_it = obj.FindMember("Processes");
  if(proc_it == obj.MemberEnd() || !proc_it->value.IsArray())
    return false;

  auto cables_it = obj.FindMember("Cables");
  if(cables_it == obj.MemberEnd() || !cables_it->value.IsArray())
    return false;

  const auto processes = proc_it->value.GetArray();
  if(processes.Empty())
    return true;

  // Find max duration
  TimeVal t = TimeVal::fromMsecs(10);
  for(auto& proc : processes)
  {
    if(!proc.IsObject())
      return false;

    auto dur = proc.FindMember("Duration");
    if(dur == proc.MemberEnd())
      return false;
    if(!dur->value.IsNumber())
      return false;
    auto d = dur->value.GetDouble();
    if(d > t.impl)
      t.impl = d;
  }

  auto& interval
      = m.createBox(sm, origin.date, TimeVal(origin.date.impl + t.impl), origin.y);
  if(startsOnPlay)
    addStartOnPlayTrigger(m, Scenario::startTimeSync(interval, sm));

  // Load each process into a slot, collecting old→new ID mapping for cable remapping.
  std::vector<std::pair<int32_t, int32_t>> proc_id_map;
  const auto pasted = loadCopiedProcesses(m, interval, obj, proc_id_map, ctx);

  // Remap cable endpoints from old process IDs to new ones, then load.
  {
    auto cables = JsonValue{cables_it->value}.to<Dataflow::SerializedCables>();
    auto new_path = score::IDocument::path(interval).unsafePath();
    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

    for(auto& [cable_id, cable_data] : cables)
    {
      auto remap = [&](ObjectPath& path) {
        auto& vec = path.vec();
        if(vec.empty())
          return;
        for(auto& [old_id, new_id] : proc_id_map)
        {
          if(vec.front().id() == old_id)
          {
            vec.front() = ObjectIdentifier{vec.front().objectName(), new_id};
            break;
          }
        }
      };
      remap(cable_data.source.unsafePath());
      remap(cable_data.sink.unsafePath());
      cable_id = getStrongId(document.cables);
    }
    m.loadCables(new_path, cables);
  }
  m.submit(new Scenario::Command::FollowPasted{std::move(pasted)});

  m.commit();

  return true;
}

static bool pasteInScenario(
    QPoint pos, ScenarioPresenter& pres, const QMimeData& mime,
    const score::DocumentContext& ctx)
{
  auto& sm = static_cast<const Scenario::ProcessModel&>(pres.model());
  ScenarioView& sv = pres.view();

  auto sv_pt = mapPointToItem(pos, sv);
  if(!sv_pt)
    return false;

  // TODO this is a bit lazy.. find a better positioning algorithm
  if(!sv.contains(*sv_pt))
    sv_pt = sv.boundingRect().center();

  // Read the copy json. TODO: give it a better mime type
  auto origin = pres.toScenarioPoint(*sv_pt);
  auto obj = readJson(mime.data("text/plain"));

  if(!obj.IsObject() || obj.MemberCount() == 0)
    return false;
  if(obj.HasMember("TimeNodes"))
  {

    // TODO check json validity
    // Submit the paste command
    auto cmd = new Command::ScenarioPasteElements(sm, obj, origin);
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);
    return true;
  }
  else if(obj.HasMember("Processes") && obj.HasMember("Cables"))
  {
    return pasteProcessesInNewBox(sm, origin, obj, ctx);
  }
  else
  {
    return false;
  }
}

static bool pasteInInterval(
    Scenario::IntervalModel& itv, QPointF item_pt, const QMimeData& mime,
    const score::DocumentContext& ctx)
{
  auto obj = readJson(mime.data("text/plain"));
  if(!obj.IsObject())
    return false;

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

    auto cmd = new Scenario::Command::PasteProcessesInInterval{
        obj, itv, ExpandMode{}, item_pt};
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);

    // FIXME paste all cables recursively ! e.g. check copy-pasting a scenario
    return true;
  }
  return false;
}

static bool pasteInCurrentInterval(
    QPoint pos, const QMimeData& mime, const score::DocumentContext& ctx)
{
  auto pres
      = score::IDocument::presenterDelegate<ScenarioDocumentPresenter>(ctx.document);
  if(!pres)
    return false;
  auto& itv = pres->displayedInterval();

  // Get the QGraphicsView
  auto view = &pres->view().view();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(pos);
  auto scene_pt = view->mapToScene(view_pt);

  // We may have to paste in the nodal view or in the temporal view,
  // with different outcomes
  struct NodalPositionVisitor
  {
    QPointF& p;
    QPointF operator()(const NodalIntervalView& nodal) const
    {
      return nodal.pastePosition(p);
    }

    QPointF operator()(const CentralIntervalDisplay& disp) const
    {
      auto itv_pres = disp.presenter.intervalPresenter();
      const auto& slots = itv_pres->getSlots();
      auto nodal_it = ossia::find_if(
          slots, [](const SlotPresenter& slot) { return bool(slot.getNodalSlot()); });
      if(nodal_it == slots.end())
        return QPointF{};

      auto nodal = nodal_it->getNodalSlot();
      if(nodal && nodal->view)
        return (*this)(*nodal->view);

      return QPointF{};
    }

    QPointF operator()(const CentralNodalDisplay& disp) const
    {
      return (*this)(*disp.presenter);
    }

    QPointF operator()(ossia::monostate) const { return QPointF{}; }
  };

  auto item_pt = ossia::visit(NodalPositionVisitor{scene_pt}, pres->display());

  return pasteInInterval(itv, item_pt, mime, ctx);
}

namespace
{
enum class PasteKind
{
  None,
  //! Processes and the cables between them: go in an interval or in a new
  //! scenario box.
  Processes,
  //! Intervals, states, events, syncs: only go in a scenario.
  Elements
};

PasteKind pasteKind(const QMimeData& mime)
{
  auto obj = readJson(mime.data("text/plain"));
  if(!obj.IsObject() || obj.MemberCount() == 0)
    return PasteKind::None;
  if(obj.HasMember("TimeNodes"))
    return PasteKind::Elements;
  if(obj.HasMember("Processes") && obj.HasMember("Cables"))
    return PasteKind::Processes;
  return PasteKind::None;
}

//! Center for processes pasted in an interval without a pointer position: the
//! first row of its nodal canvas, after the existing nodes, so they are visible
//! even in a small slot.
QPointF pastePositionInInterval(const IntervalModel& itv, const QMimeData& mime)
{
  QSizeF size{75., 42.};
  auto obj = readJson(mime.data("text/plain"));
  if(obj.IsObject())
    if(auto it = obj.FindMember("Processes");
       it != obj.MemberEnd() && it->value.IsArray() && !it->value.Empty())
      size = Command::copiedProcessesRect(it->value.GetArray()).size();

  const QPointF topLeft = freeProcessPosition(itv, QPointF{40., 40.}, size);
  return topLeft + QPointF{size.width() / 2., size.height() / 2.};
}

//! The nodal canvas that shows `itv` in `view`, if one is visible: a child
//! interval's slot in the small view.
const NodalIntervalView*
shownNodalView(const QGraphicsView& view, const IntervalModel& itv)
{
  for(QGraphicsItem* item : view.scene()->items())
    if(item->type() == ItemType::Type::NodalIntervalView && item->isVisible())
      if(auto nodal = static_cast<const NodalIntervalView*>(item);
         &nodal->model() == &itv)
        return nodal;
  return nullptr;
}

//! Where in a scenario to put what is pasted next to one of its children.
Scenario::Point pointNextTo(const Scenario::ProcessModel& sc, const QObject* child)
{
  if(auto itv = qobject_cast<const IntervalModel*>(child))
    return {itv->date(), std::min(itv->heightPercentage() + 0.05, 0.95)};
  if(auto st = qobject_cast<const StateModel*>(child))
  {
    auto& ev = Scenario::parentEvent(*st, sc);
    return {ev.date(), std::min(st->heightPercentage() + 0.05, 0.95)};
  }
  if(auto ev = qobject_cast<const EventModel*>(child))
    return {ev->date(), 0.1};
  return {TimeVal::zero(), 0.1};
}
}

bool ScenarioEditor::paste(
    QPoint pos, QObject* focusedObject, const QMimeData& mime,
    const score::DocumentContext& ctx)
{
  auto pres
      = score::IDocument::presenterDelegate<ScenarioDocumentPresenter>(ctx.document);
  if(!pres)
    return false;
  auto& displayed = pres->displayedInterval();

  const auto kind = pasteKind(mime);
  if(kind == PasteKind::None)
    return false;

  // Start from whichever of the selection and the focus changed last.
  auto focusedLayer = qobject_cast<Process::LayerPresenter*>(focusedObject);
  QObject* focused
      = focusedLayer ? const_cast<Process::ProcessModel*>(&focusedLayer->model())
                     : nullptr;
  QObject* selected{};
  if(auto sel = ctx.selectionStack.currentSelection(); sel.size() == 1)
    selected = sel.at(0);

  QObject* start = focused;
  if(!start)
    start = selected;
  else if(selected && selected != start)
  {
    auto focusManager = Process::ProcessFocusManager::get(ctx);
    if(focusManager && focusManager->selectionNewerThanFocus())
      start = selected;
  }

  // The first ancestor that can take what was copied receives it, next to the
  // child it was reached from.
  QObject* child{};
  for(QObject* obj = start; obj; child = obj, obj = obj->parent())
  {
    if(auto sc = qobject_cast<Scenario::ProcessModel*>(obj))
    {
      // Processes go in a new box at the pointer, which needs the scenario's
      // view; a scenario that is not shown gets them next to the child.
      auto scenarioPres = qobject_cast<ScenarioPresenter*>(focusedObject);
      const bool shown = scenarioPres && &scenarioPres->model() == sc;
      if(kind == PasteKind::Elements || shown)
      {
        if(shown && obj == start)
          return pasteInScenario(pos, *scenarioPres, mime, ctx);

        auto json = readJson(mime.data("text/plain"));
        const auto origin = pointNextTo(*sc, child);
        if(kind == PasteKind::Elements)
        {
          CommandDispatcher<>{ctx.commandStack}.submit(
              new Command::ScenarioPasteElements(*sc, json, origin));
          return true;
        }
        return pasteProcessesInNewBox(*sc, origin, json, ctx);
      }
    }
    else if(auto itv = qobject_cast<IntervalModel*>(obj); itv && kind == PasteKind::Processes)
    {
      if(itv == &displayed)
        return pasteInCurrentInterval(pos, mime, ctx);
      const auto& view = pres->view().view();
      if(auto nodal = shownNodalView(view, *itv))
        return pasteInInterval(
            *itv, nodal->pastePosition(view.mapToScene(view.mapFromGlobal(pos))), mime,
            ctx);
      return pasteInInterval(*itv, pastePositionInInterval(*itv, mime), mime, ctx);
    }
    else if(qobject_cast<ScenarioDocumentModel*>(obj))
    {
      break;
    }
  }

  if(kind == PasteKind::Processes)
    return pasteInCurrentInterval(pos, mime, ctx);
  return false;
}

namespace
{
//! The ports through which a process sits in a chain: its first inlet and
//! first outlet, when they carry the same kind of data.
const Process::Inlet* chainInlet(const Process::ProcessModel& p) noexcept
{
  auto& is = p.inlets();
  auto& os = p.outlets();
  if(is.empty() || os.empty() || is[0]->type() != os[0]->type())
    return nullptr;
  return is[0];
}

const Process::Outlet* chainOutlet(const Process::ProcessModel& p) noexcept
{
  auto& is = p.inlets();
  auto& os = p.outlets();
  if(is.empty() || os.empty() || is[0]->type() != os[0]->type())
    return nullptr;
  return os[0];
}

struct RemovalGraph
{
  const score::DocumentContext& ctx;
  const std::vector<const Process::ProcessModel*>& removed;

  bool isRemoved(const Process::ProcessModel* p) const noexcept
  {
    return ossia::contains(removed, p);
  }

  //! Whether the object goes away with one of the removed processes, as a
  //! port of it or of a process it contains.
  bool goesAway(const QObject* obj) const noexcept
  {
    for(; obj; obj = obj->parent())
      if(auto p = qobject_cast<const Process::ProcessModel*>(obj); p && isRemoved(p))
        return true;
    return false;
  }

  //! The removed process whose chain inlet this is.
  const Process::ProcessModel* removedThroughInlet(const Process::Port& port) const noexcept
  {
    auto p = qobject_cast<const Process::ProcessModel*>(port.parent());
    return p && isRemoved(p) && chainInlet(*p) == &port ? p : nullptr;
  }

  const Process::ProcessModel* removedThroughOutlet(const Process::Port& port) const noexcept
  {
    auto p = qobject_cast<const Process::ProcessModel*>(port.parent());
    return p && isRemoved(p) && chainOutlet(*p) == &port ? p : nullptr;
  }

  struct Sink
  {
    const Process::Inlet* inlet{};
    Process::CableType type{};
  };

  //! The inlets that stay and that `p` fed, through the removed processes
  //! chained after it.
  void sinksAfter(
      const Process::ProcessModel& p, std::vector<const Process::ProcessModel*>& visited,
      std::vector<Sink>& out) const
  {
    for(auto& path : chainOutlet(p)->cables())
    {
      auto& cbl = path.find(ctx);
      auto& sink = cbl.sink().find(ctx);
      if(auto next = removedThroughInlet(sink))
      {
        if(!ossia::contains(visited, next))
        {
          visited.push_back(next);
          sinksAfter(*next, visited, out);
        }
      }
      else if(!goesAway(&sink))
      {
        if(auto in = qobject_cast<const Process::Inlet*>(&sink))
          if(!ossia::any_of(out, [in](const Sink& s) { return s.inlet == in; }))
            out.push_back({in, cbl.type()});
      }
    }
  }

  //! The outlets that stay and that fed `p`, through the removed processes
  //! chained before it.
  void sourcesBefore(
      const Process::ProcessModel& p, std::vector<const Process::ProcessModel*>& visited,
      std::vector<const Process::Outlet*>& out) const
  {
    for(auto& path : chainInlet(p)->cables())
    {
      auto& src = path.find(ctx).source().find(ctx);
      if(auto prev = removedThroughOutlet(src))
      {
        if(!ossia::contains(visited, prev))
        {
          visited.push_back(prev);
          sourcesBefore(*prev, visited, out);
        }
      }
      else if(!goesAway(&src))
      {
        if(auto o = qobject_cast<const Process::Outlet*>(&src))
          if(!ossia::contains(out, o))
            out.push_back(o);
      }
    }
  }

  //! Whether a cable from `src` to `sink` stays once the processes are gone.
  bool linked(const Process::Outlet& src, const Process::Inlet& sink) const
  {
    for(auto& path : src.cables())
      if(&path.find(ctx).sink().find(ctx) == &sink)
        return true;
    return false;
  }

  //! Whether `src` still has a cable once the processes are gone.
  bool keepsACable(const Process::Outlet& src) const
  {
    for(auto& path : src.cables())
      if(!goesAway(&path.find(ctx).sink().find(ctx)))
        return true;
    return false;
  }
};
}

void removeIntervalProcesses(
    const std::vector<const Process::ProcessModel*>& selected, RemovalLinks links,
    const score::DocumentContext& ctx)
{
  // A process inside another removed one goes away with it
  std::vector<const Process::ProcessModel*> procs;
  for(auto* p : selected)
  {
    bool nested = false;
    for(auto* o = p->parent(); o && !nested; o = o->parent())
      nested = ossia::contains(selected, qobject_cast<const Process::ProcessModel*>(o));
    if(!nested && !ossia::contains(procs, p))
      procs.push_back(p);
  }
  if(procs.empty())
    return;

  auto& doc = score::IDocument::get<ScenarioDocumentModel>(ctx.document);
  const bool reconnect = links == RemovalLinks::Reconnect;
  const RemovalGraph g{ctx, procs};

  struct NewCable
  {
    const Process::Outlet* source{};
    const Process::Inlet* sink{};
    Process::CableType type{};
  };
  std::vector<NewCable> cables;
  std::vector<std::pair<const Process::Inlet*, State::AddressAccessor>> addresses;
  std::vector<std::pair<const Process::Outlet*, OutletRouting>> routings;

  if(reconnect)
  {
    for(auto* p : procs)
    {
      auto* in = chainInlet(*p);
      if(!in)
        continue;

      std::vector<const Process::ProcessModel*> visited{p};
      std::vector<RemovalGraph::Sink> sinks;
      g.sinksAfter(*p, visited, sinks);
      if(sinks.empty())
        continue;

      if(in->cables().empty())
      {
        // The head of a chain gives its address to what it fed
        if(in->address().isSet())
          for(auto& s : sinks)
            if(!ossia::any_of(addresses, [&](auto& a) { return a.first == s.inlet; }))
              addresses.emplace_back(s.inlet, in->address());
        continue;
      }

      // What fed the removed sub-chain now feeds what it fed
      for(auto& path : in->cables())
      {
        auto& src_port = path.find(ctx).source().find(ctx);
        auto* src = qobject_cast<const Process::Outlet*>(&src_port);
        if(!src || g.goesAway(src))
          continue;
        for(auto& s : sinks)
        {
          if(g.linked(*src, *s.inlet))
            continue;
          if(ossia::any_of(cables, [&](const NewCable& c) {
               return c.source == src && c.sink == s.inlet;
             }))
            continue;
          cables.push_back({src, s.inlet, s.type});
        }
      }
    }
  }

  // The tail of a chain going away hands its routing back to what fed it.
  for(auto* p : procs)
  {
    auto* out = chainOutlet(*p);
    if(!out || !out->cables().empty())
      continue;

    auto routing = outletRouting(*out);
    if(!reconnect)
      routing.address = State::AddressAccessor{};
    if(routing.address == State::AddressAccessor{} && !routing.propagate)
      continue;

    std::vector<const Process::ProcessModel*> visited{p};
    std::vector<const Process::Outlet*> sources;
    g.sourcesBefore(*p, visited, sources);
    for(auto* src : sources)
    {
      if(g.keepsACable(*src))
        continue;
      if(ossia::any_of(cables, [src](const NewCable& c) { return c.source == src; }))
        continue;
      if(ossia::any_of(routings, [src](auto& r) { return r.first == src; }))
        continue;
      routings.emplace_back(src, routing);
    }
  }

  if(procs.size() == 1 && cables.empty() && addresses.empty() && routings.empty())
  {
    auto& itv = *safe_cast<IntervalModel*>(procs.front()->parent());
    CommandDispatcher<> d{ctx.commandStack};
    d.submit<Command::RemoveProcessFromInterval>(itv, procs.front()->id());
    return;
  }

  Command::Macro m{
      procs.size() == 1
          ? static_cast<score::AggregateCommand*>(new Command::RemoveProcessAndKeepLinked)
          : new Command::RemoveMultipleProcessesFromInterval,
      ctx};
  for(auto& c : cables)
    m.createCable(doc, *c.source, *c.sink, c.type);
  for(auto& [inlet, addr] : addresses)
    m.setProperty<Process::Port::p_address>(*inlet, addr);
  for(auto& [outlet, routing] : routings)
    applyOutletRouting(m, *outlet, routing);
  for(auto* p : procs)
    m.removeProcess(*safe_cast<IntervalModel*>(p->parent()), p->id());
  m.commit();
}

bool ScenarioEditor::remove(
    const Selection& s, const score::DocumentContext& ctx, RemovalLinks links)
{
  if(s.size() > 1)
  {
    std::vector<const Process::ProcessModel*> procs;
    procs.reserve(s.size());
    bool only_interval_processes = true;
    for(auto& elt : s)
    {
      auto proc = qobject_cast<const Process::ProcessModel*>(elt.data());
      if(!proc || !qobject_cast<IntervalModel*>(proc->parent()))
      {
        only_interval_processes = false;
        break;
      }
      procs.push_back(proc);
    }

    if(only_interval_processes && !procs.empty())
    {
      removeIntervalProcesses(procs, links, ctx);
      return true;
    }
  }

  if(s.size() == 1)
  {
    auto first = s.begin()->data();
    if(auto c = qobject_cast<const Process::Cable*>(first))
    {
      CommandDispatcher<> d{ctx.commandStack};
      auto& doc = score::IDocument::get<ScenarioDocumentModel>(ctx.document);
      d.submit<Dataflow::RemoveCable>(doc, *c);
      return true;
    }
    else if(auto proc = qobject_cast<const Process::ProcessModel*>(first))
    {
      using namespace Command;
      auto p = proc->parent();
      if(qobject_cast<IntervalModel*>(p))
      {
        removeIntervalProcesses({proc}, links, ctx);
        return true;
      }
      else if(auto st = qobject_cast<StateModel*>(p))
      {
        CommandDispatcher<> d{ctx.commandStack};
        d.submit<RemoveStateProcess>(*st, proc->id());
        return true;
      }
      else
      {
        return false;
      }
    }
  }

  if(auto sm = focusedScenarioModel(ctx))
  {
    if(s.size() == 1)
    {
      auto first = s.begin()->data();
      if(auto cb = qobject_cast<const Scenario::CommentBlockModel*>(first))
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
  if(si)
  {
    Scenario::clearContentFromSelection(*si, ctx);
    return true;
  }

  return false;
}
}
