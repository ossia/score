// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationDropHandler.hpp"

#include <Dataflow/Commands/CableHelpers.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/thread.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QFileInfo>
#include <QUrl>
namespace Scenario
{
class DropProcessOnStateHelper
{
public:
  DropProcessOnStateHelper(
      const StateModel& sourceState,
      const Scenario::ProcessModel& scenar,
      const score::DocumentContext& ctx,
      TimeVal maxdur)
      : m_sequence{bool(qApp->keyboardModifiers() & Qt::ShiftModifier)}
      , m_scenar{scenar}
      , m_macro{new Command::AddProcessInNewBoxMacro, ctx}
  {
    auto& m = m_macro;

    const auto& parent_ev = Scenario::parentEvent(sourceState, scenar);
    const auto& date = parent_ev.date();
    m_currentDate = date;
    if (!m_sequence)
    {
      if (!sourceState.nextInterval())
      {
        m_intervalY = sourceState.heightPercentage();
        // Everything will go in a single interval
        m_itv = &m.createIntervalAfter(
            scenar, sourceState.id(), Scenario::Point{date + maxdur, m_intervalY});
      }
      else
      {
        m_intervalY = sourceState.heightPercentage() + 0.1;
        m_createdState = m.createState(scenar, parent_ev.id(), m_intervalY).id();
        m_itv = &m.createIntervalAfter(
            scenar, m_createdState, Scenario::Point{date + maxdur, m_intervalY});
      }
    }
    else
    {
      if (!sourceState.nextInterval())
      {
        m_intervalY = sourceState.heightPercentage();
        m_createdState = sourceState.id();
      }
      else
      {
        m_intervalY = sourceState.heightPercentage() + 0.1;
        m_createdState = m.createState(scenar, parent_ev.id(), m_intervalY).id();
      }
    }
  }

  template <typename F>
  Process::ProcessModel* addProcess(F&& fun, TimeVal duration)
  {
    // sequence : processes are put all one after the other
    if (m_sequence)
    {
      {
        // We create the first interval / process
        m_currentDate += duration;
        m_itv = &m_macro.createIntervalAfter(
            m_scenar, m_createdState, Scenario::Point{m_currentDate, m_intervalY});
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
    }
    else
    {
      SCORE_ASSERT(m_itv);
      return fun(m_macro, *m_itv);
    }
  }

  void commit()
  {
    if (!m_sequence)
    {
      SCORE_ASSERT(m_itv);
      m_macro.showRack(*m_itv);
    }

    m_macro.commit();
  }

  Scenario::IntervalModel& interval() { return *m_itv; }
  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const bool m_sequence{};
  double m_intervalY{};
  TimeVal m_currentDate{};
  const Scenario::ProcessModel& m_scenar;
  QPointF m_pos;
  Scenario::Command::Macro m_macro;
  Scenario::IntervalModel* m_itv{};
  Id<StateModel> m_createdState;
};

bool DropProcessOnState::drop(
    const StateModel& st,
    const ProcessModel& scenar,
    const QMimeData& mime,
    const score::DocumentContext& ctx)
{

  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));

    DropProcessOnStateHelper dropper(st, scenar, ctx, t);

    score::Dispatcher_T disp{dropper.macro()};
    for (const auto& proc : res)
    {
      Process::ProcessModel* p = dropper.addProcess(
          [&](Scenario::Command::Macro& m, const IntervalModel& itv) -> Process::ProcessModel* {
            auto p
                = m.createProcessInNewSlot(itv, proc.creation.key, proc.creation.customData, {});
            if (auto& name = proc.creation.prettyName; !name.isEmpty())
              dropper.macro().submit(new Scenario::Command::ChangeElementName{*p, name});
            return p;
          },
          proc.duration ? *proc.duration : t);
      if (p && proc.setup)
      {
        proc.setup(*p, disp);
      }
    }

    if (res.size() == 1)
    {
      const auto& name = res.front().creation.prettyName;
      auto& itv = dropper.interval();
      if (!name.isEmpty())
      {
        dropper.macro().submit(new Scenario::Command::ChangeElementName{itv, name});
      }
    }
    dropper.commit();
    return true;
  }
  return true;
}

class DropProcessInScenarioHelper
{
public:
  DropProcessInScenarioHelper(
      MagneticStates m_magnetic,
      const Scenario::ScenarioPresenter& pres,
      QPointF pos,
      TimeVal maxdur)
      : m_sequence{bool(qApp->keyboardModifiers() & Qt::ShiftModifier)}
      , m_pres{pres}
      , m_pos{pos}
      , m_macro{new Command::AddProcessInNewBoxMacro, pres.context().context}
  {
    auto& m = m_macro;
    const auto& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    auto [x_state, y_state, magnetic] = m_magnetic;
    if (y_state)
    {
      m_intervalY = pt.y;
      if (magnetic || pt.date <= scenar.event(y_state->eventId()).date())
      {
        // Create another state on that event and put the process afterwards
        m_createdState = m.createState(scenar, y_state->eventId(), m_intervalY).id();
      }
      else
      {
        auto& s = m.createState(scenar, y_state->eventId(), m_intervalY);
        auto& i = m.createIntervalAfter(scenar, s.id(), {pt.date, m_intervalY});
        m_createdState = i.endState();
      }
    }
    else if (x_state)
    {
      if (x_state->nextInterval())
      {
        // We create from the event instead
        m_intervalY = pt.y;
        auto& s = m.createState(scenar, x_state->eventId(), m_intervalY);
        auto& i = m.createIntervalAfter(scenar, s.id(), {pt.date, m_intervalY});
        m_createdState = i.endState();
      }
      else
      {
        m_intervalY = magnetic ? x_state->heightPercentage() : pt.y;

        auto& i = m.createIntervalAfter(scenar, x_state->id(), {pt.date, m_intervalY});
        m_createdState = i.endState();
      }
    }
    else
    {
      // We create in the emptiness
      const auto& [t, e, s] = m.createDot(scenar, pt);
      m_createdState = s.id();
    }

    if (!m_sequence)
    {
      // Everything will go in a single interval
      m_itv = &m.createIntervalAfter(
          scenar, m_createdState, Scenario::Point{pt.date + maxdur, m_intervalY});
    }
  }

  template <typename F>
  Process::ProcessModel* addProcess(F&& fun, TimeVal duration)
  {
    // sequence : processes are put all one after the other
    if (m_sequence)
    {
      const Scenario::ProcessModel& scenar = m_pres.model();
      Scenario::Point pt = m_pres.toScenarioPoint(m_pos);
      if (m_itv)
      {
        // We already created the first interval / process
        auto last_state = m_itv->endState();
        pt.date = Scenario::parentEvent(scenar.state(last_state), scenar).date() + duration;
        m_itv = &m_macro.createIntervalAfter(scenar, last_state, {pt.date, m_intervalY});
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
      else
      {
        // We create the first interval / process
        m_itv = &m_macro.createIntervalAfter(scenar, m_createdState, pt);
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
    }
    else
    {
      return fun(m_macro, *m_itv);
    }
  }

  void commit()
  {
    if (!m_sequence)
      m_macro.showRack(*m_itv);

    m_macro.commit();
  }

  Scenario::IntervalModel& interval() { return *m_itv; }
  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const bool m_sequence{};
  double m_intervalY{};
  const Scenario::ScenarioPresenter& m_pres;
  QPointF m_pos;
  Scenario::Command::Macro m_macro;
  Scenario::IntervalModel* m_itv{};
  Id<StateModel> m_createdState;
};

class DropProcessInIntervalHelper
{
public:
  DropProcessInIntervalHelper(const Scenario::IntervalModel& interval, std::optional<TimeVal> maxdur)
      : m_context{score::IDocument::documentContext(interval)}
      , m_macro{new Command::AddProcessInNewBoxMacro, m_context}
      , m_itv{interval}
  {
    // If the interval has no processes and nothing after, we will resize it
    if (interval.processes.empty() && maxdur)
    {
      if (auto resizer
          = m_context.app.interfaces<Scenario::IntervalResizerList>().make(m_itv, *maxdur))
        m_macro.submit(resizer);
    }
  }

  template <typename F>
  Process::ProcessModel* addProcess(F&& fun)
  {
    return fun(m_macro, m_itv);
  }

  void commit()
  {
    m_macro.showRack(m_itv);
    m_macro.commit();
  }

  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const score::DocumentContext& m_context;
  Scenario::Command::Macro m_macro;
  const Scenario::IntervalModel& m_itv;
};

DropProcessInScenario::DropProcessInScenario() { }

void DropProcessInScenario::init()
{
  const auto& handlers = score::GUIAppContext().interfaces<Process::ProcessDropHandlerList>();
  for (auto& handler : handlers)
  {
    for (auto& type : handler.mimeTypes())
      m_acceptableMimeTypes.push_back(type);
    for (auto& ext : handler.fileExtensions())
      m_acceptableSuffixes.push_back(ext);
  }
}
bool DropProcessInScenario::drop(const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  const auto& ctx = pres.context().context;
  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));

    DropProcessInScenarioHelper dropper(m_magnetic, pres, pos, t);

    score::Dispatcher_T disp{dropper.macro()};
    for (const auto& proc : res)
    {
      Process::ProcessModel* p = dropper.addProcess(
          [&](Scenario::Command::Macro& m, const IntervalModel& itv) -> Process::ProcessModel* {
            auto p
                = m.createProcessInNewSlot(itv, proc.creation.key, proc.creation.customData, {});
            if (auto& name = proc.creation.prettyName; !name.isEmpty())
              dropper.macro().submit(new Scenario::Command::ChangeElementName{*p, name});
            return p;
          },
          proc.duration ? *proc.duration : t);
      if (p && proc.setup)
      {
        proc.setup(*p, disp);
      }
    }

    if (res.size() == 1)
    {
      const auto& name = res.front().creation.prettyName;
      auto& itv = dropper.interval();
      if (!name.isEmpty())
      {
        dropper.macro().submit(new Scenario::Command::ChangeElementName{itv, name});
      }
    }

    dropper.commit();
    return true;
  }
  return false;
}

bool DropProcessInInterval::drop(
    const score::DocumentContext& ctx,
    const IntervalModel& cst,
    QPointF pos,
    const QMimeData& mime)
{
  if (mime.hasFormat("score/object-item-model-index"))
  {
    auto dat = score::unmarshall<Path<Process::ProcessModel>>(
        mime.data("score/object-item-model-index"));
    auto proc = dat.try_find(ctx);
    if (proc->parent() == &cst)
    {
      CommandDispatcher<>{ctx.commandStack}.submit(
          new Scenario::Command::AddLayerInNewSlot{cst, proc->id()});
    }
    return true;
  }

  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res);
    DropProcessInIntervalHelper dropper(cst, t);

    score::Dispatcher_T disp{dropper.macro()};
    for (const auto& proc : res)
    {
      auto p = dropper.addProcess(
          [&](Scenario::Command::Macro& m, const IntervalModel& itv) -> Process::ProcessModel* {
            return m.createProcessInNewSlot(itv, proc.creation.key, proc.creation.customData, pos);
          });
      if (p && proc.setup)
      {
        proc.setup(*p, disp);
      }
    }

    dropper.commit();
    return true;
  }
  return false;
}

DropScenario::DropScenario()
{
  // TODO give them a mime type ?
  m_acceptableSuffixes.push_back("scenario");
}

bool DropScenario::drop(const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if (mime.hasUrls())
  {
    const auto& doc = pres.context().context;
    auto& sm = pres.model();
    auto path = mime.urls().first().toLocalFile();
    if (QFile f{path}; QFileInfo{f}.suffix() == "scenario" && f.open(QIODevice::ReadOnly))
    {
      CommandDispatcher<> d{doc.commandStack};
      d.submit(new Scenario::Command::ScenarioPasteElements(
          sm, readJson(f.readAll()), pres.toScenarioPoint(pos)));
      return true;
    }
  }

  return false;
}

DropScore::DropScore()
{
  m_acceptableSuffixes.push_back("score");
  m_acceptableSuffixes.push_back("scorebin");
}

bool DropScore::drop(const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if (mime.hasUrls())
  {
    const auto& doc = pres.context().context;
    auto& sm = pres.model();
    auto path = mime.urls().first().toLocalFile();
    if (QFile f{path}; QFileInfo{f}.suffix() == "score" && f.open(QIODevice::ReadOnly))
    {
      rapidjson::Document res;

      auto obj = readJson(f.readAll());
      auto& docobj = obj["Document"];
      res["Cables"] = docobj["Cables"];

      auto& scenar = docobj["BaseScenario"];

      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["Constraint"], obj.GetAllocator());
        res["Intervals"] = arr;
      }
      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["StartState"], obj.GetAllocator());
        arr.PushBack(scenar["EndState"], obj.GetAllocator());
        res["States"] = arr;
      }
      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["StartEvent"], obj.GetAllocator());
        arr.PushBack(scenar["EndEvent"], obj.GetAllocator());
        res["Events"] = arr;
      }
      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["StartTimeNode"], obj.GetAllocator());
        arr.PushBack(scenar["EndTimeNode"], obj.GetAllocator());
        res["TimeNodes"] = arr;
      }

      CommandDispatcher<> d{doc.commandStack};
      d.submit(new Scenario::Command::ScenarioPasteElements(sm, res, pres.toScenarioPoint(pos)));
      return true;
    }
  }

  return false;
}

DropLayerInScenario::DropLayerInScenario()
{
  m_acceptableMimeTypes.push_back(score::mime::layerdata());
  m_acceptableSuffixes.push_back("layer");
}

bool DropLayerInScenario::drop(const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  rapidjson::Document json;
  QString filename;
  if (mime.formats().contains(score::mime::layerdata()))
  {
    json = readJson(mime.data(score::mime::layerdata()));
  }
  else if (mime.hasUrls())
  {
    if (QFile f{mime.urls()[0].toLocalFile()};
        QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
    {
      filename = QFileInfo{f}.fileName();
      json = readJson(f.readAll());
    }
  }
  else
  {
    return false;
  }

  if (!json.IsObject() || json.MemberCount() == 0)
    return false;
  if(!json.HasMember("Path") || !json.HasMember("Duration"))
    return false;

  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  const TimeVal t = TimeVal::fromMsecs(json["Duration"].GetDouble());

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  DropLayerInInterval::perform(interval, pres.context().context, m, json);

  m.submit(new Scenario::Command::ChangeElementName{interval, filename});
  m.commit();
  return true;
}



DropPresetInScenario::DropPresetInScenario()
{
  m_acceptableSuffixes.push_back("scorepreset");
}

bool DropPresetInScenario::drop(const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  rapidjson::Document json;
  QString filename;
  if (mime.hasUrls())
  {
    if (QFile f{mime.urls()[0].toLocalFile()};
        QFileInfo{f}.suffix() == "scorepreset" && f.open(QIODevice::ReadOnly))
    {
      filename = QFileInfo{f}.fileName();
      json = readJson(f.readAll());
    }
  }
  else
  {
    return false;
  }

  if (!json.IsObject() || json.MemberCount() == 0)
    return false;

  /*
  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  const TimeVal t = TimeVal::fromMsecs(json["Duration"].GetDouble());

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  DropPresetInInterval::perform(interval, pres.context().context, m, json);

  m.submit(new Scenario::Command::ChangeElementName{interval, filename});
  m.commit();
  */
  return true;
}



void DropLayerInInterval::perform(
    const IntervalModel& interval,
    const score::DocumentContext& ctx,
    Scenario::Command::Macro& m,
    const rapidjson::Document& json)
{
  const auto pid = ossia::get_pid();
  bool same_doc = false;

  if (!json.HasMember("Path") || !json.HasMember("Cables"))
  {
    // TODO this is the "move the nodal slot" case
    return;
  }
  if (json.HasMember("PID") && json.HasMember("Document"))
  {
    same_doc = (pid == json["PID"].GetInt());
    same_doc &= (ctx.document.id().val() == json["Document"].GetInt());
  }
  const bool small_view = JsonValue{json["View"]}.toString() == "Small";
  const int slot_index = json["SlotIndex"].GetInt();

  if (same_doc)
  {
    auto old_p = JsonValue{json["Path"]}.to<Path<Process::ProcessModel>>();
    if (auto obj = old_p.try_find(ctx))
      if (auto itv = qobject_cast<IntervalModel*>(obj->parent()))
      {
        if (small_view && (qApp->keyboardModifiers() & Qt::ALT))
        {
          m.moveSlot(*itv, interval, slot_index);
        }
        else
        {
          m.moveProcess(*itv, interval, obj->id());
        }

        if (itv->processes.empty())
        {
          if (auto sm = dynamic_cast<Scenario::ProcessModel*>(itv->parent()))
          {
            auto& es = Scenario::endState(*itv, *sm);
            if (es.empty() && !es.nextInterval())
            {
              m.removeElements(*sm, Selection{itv, &es});
            }
          }
        }
      }
  }
  else
  {
    // Just create a new process
    m.loadProcessInSlot(interval, json);
  }

  // Reload cables
  {
    auto new_path = score::IDocument::path(interval).unsafePath();
    auto cables = JsonValue{json["Cables"]}.to<Dataflow::SerializedCables>();

    auto& document = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

    for (auto& c : cables)
    {
      c.first = getStrongId(document.cables);
    }
    m.loadCables(new_path, cables);
  }

  // Finally we show the newly created rack
  m.showRack(interval);
}

bool DropLayerInInterval::drop(
    const score::DocumentContext& ctx,
    const IntervalModel& interval,
    QPointF p,
    const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::layerdata()))
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};

    const auto json = readJson(mime.data(score::mime::layerdata()));
    perform(interval, ctx, m, json);
    m.commit();
    return true;
  }
  else if (mime.hasUrls())
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};
    bool ok = false;
    for (const QUrl& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if (QFile f{path}; QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
      {
        ok = true;
        perform(interval, ctx, m, readJson(f.readAll()));
      }
    }

    if (ok)
      m.commit();
    return true;
  }

  return false;
}



void DropPresetInInterval::perform(
    const IntervalModel& interval,
    const score::DocumentContext& ctx,
    Scenario::Command::Macro& m,
    const rapidjson::Document& json)
{
  /*
  const auto pid = ossia::get_pid();
  bool same_doc = false;
  if (json.HasMember("PID") && json.HasMember("Document"))
  {
    same_doc = (pid == json["PID"].GetInt());
    same_doc &= (ctx.document.id().val() == json["Document"].GetInt());
  }
  const bool small_view = JsonValue{json["View"]}.toString() == "Small";
  const int slot_index = json["SlotIndex"].GetInt();

  if (same_doc)
  {
    auto old_p = JsonValue{json["Path"]}.to<Path<Process::ProcessModel>>();
    if (auto obj = old_p.try_find(ctx))
      if (auto itv = qobject_cast<IntervalModel*>(obj->parent()))
      {
        if (small_view && (qApp->keyboardModifiers() & Qt::ALT))
        {
          m.moveSlot(*itv, interval, slot_index);
        }
        else
        {
          m.moveProcess(*itv, interval, obj->id());
        }

        if (itv->processes.empty())
        {
          if (auto sm = dynamic_cast<Scenario::ProcessModel*>(itv->parent()))
          {
            auto& es = Scenario::endState(*itv, *sm);
            if (es.empty() && !es.nextInterval())
            {
              m.removeElements(*sm, Selection{itv, &es});
            }
          }
        }
      }
  }
  else
  {
    // Just create a new process
    m.loadProcessInSlot(interval, json);
  }

  // Reload cables
  {
    auto new_path = score::IDocument::path(interval).unsafePath();
    auto cables = JsonValue{json["Cables"]}.to<Dataflow::SerializedCables>();

    auto& document = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

    for (auto& c : cables)
    {
      c.first = getStrongId(document.cables);
    }
    m.loadCables(new_path, cables);
  }

  // Finally we show the newly created rack
  m.showRack(interval);
  */
}

bool DropPresetInInterval::drop(
    const score::DocumentContext& ctx,
    const IntervalModel& interval,
    QPointF p,
    const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::layerdata()))
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};

    const auto json = readJson(mime.data(score::mime::layerdata()));
    perform(interval, ctx, m, json);
    m.commit();
    return true;
  }
  else if (mime.hasUrls())
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};
    bool ok = false;
    for (const QUrl& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if (QFile f{path}; QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
      {
        ok = true;
        perform(interval, ctx, m, readJson(f.readAll()));
      }
    }

    if (ok)
      m.commit();
    return true;
  }

  return false;
}


static void getAddressesRecursively(
    const Device::Node& node,
    State::Address curAddr,
    std::vector<Device::FullAddressSettings>& addresses)
{
  // TODO refactor with CreateCurves and AddressAccessorEditWidget
  if (node.is<Device::AddressSettings>())
  {
    const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
    // FIXME see https://github.com/ossia/libossia/issues/291
    if (ossia::is_numeric(addr.value) || ossia::is_array(addr.value))
    {
      Device::FullAddressSettings as;
      static_cast<Device::AddressSettingsCommon&>(as) = addr;
      as.address = curAddr;
      addresses.push_back(std::move(as));
    }
    // TODO interpolation
  }

  for (auto& child : node)
  {
    const Device::AddressSettings& addr = child.get<Device::AddressSettings>();

    State::Address newAddr{curAddr};
    newAddr.path.append(addr.name);
    getAddressesRecursively(child, newAddr, addresses);
  }
}

bool AutomationDropHandler::drop(
    const score::DocumentContext& ctx,
    const IntervalModel& cst,
    QPointF p,
    const QMimeData& mime)
{
  // TODO refactor with AddressEditWidget
  if (mime.formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();
    if (nl.empty())
      return false;

    std::vector<Device::FullAddressSettings> addresses;
    for (auto& np : nl)
    {
      getAddressesRecursively(np.second, np.first, addresses);
    }

    if (addresses.empty())
      return false;

    CreateCurvesFromAddresses({&cst}, addresses, ctx.commandStack);

    return true;
  }
  else
  {
    return false;
  }
}

}
