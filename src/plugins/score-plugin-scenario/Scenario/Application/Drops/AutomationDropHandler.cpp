// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationDropHandler.hpp"

#include <Dataflow/Commands/CableHelpers.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/thread.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QFileInfo>
namespace Scenario
{

class DropProcessInScenarioHelper
{
public:
  DropProcessInScenarioHelper(
      const Scenario::ScenarioPresenter& pres,
      QPointF pos,
      TimeVal maxdur)
      : m_sequence{bool(qApp->keyboardModifiers() & Qt::ShiftModifier)}
      , m_pres{pres}
      , m_pos{pos}
      , m_macro{new Command::AddProcessInNewBoxMacro, pres.context().context}
  {
    if (!m_sequence)
    {
      const Scenario::ProcessModel& scenar = pres.model();
      Scenario::Point pt = pres.toScenarioPoint(pos);
      m_itv = &m_macro.createBox(scenar, pt.date, pt.date + maxdur, pt.y);
    }
  }

  template <typename F>
  auto addProcess(F&& fun, TimeVal duration) -> decltype(auto)
  {
    if (m_sequence)
    {
      const Scenario::ProcessModel& scenar = m_pres.model();
      Scenario::Point pt = m_pres.toScenarioPoint(m_pos);
      if (m_itv)
      {
        auto last_state = m_itv->endState();
        pt.date
            = Scenario::parentEvent(scenar.state(last_state), scenar).date()
              + duration;
        m_itv = &m_macro.createIntervalAfter(scenar, last_state, pt);
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
      else
      {
        m_itv = &m_macro.createBox(scenar, pt.date, pt.date + duration, pt.y);
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

  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const bool m_sequence;
  const Scenario::ScenarioPresenter& m_pres;
  QPointF m_pos;
  Scenario::Command::Macro m_macro;
  Scenario::IntervalModel* m_itv{};
};

class DropProcessInIntervalHelper
{
public:
  DropProcessInIntervalHelper(
      const Scenario::IntervalModel& interval,
      optional<TimeVal> maxdur)
      : m_context{score::IDocument::documentContext(interval)}
      , m_macro{new Command::AddProcessInNewBoxMacro, m_context}
      , m_itv{interval}
  {
    // If the interval has no processes and nothing after, we will resize it
    if (interval.processes.empty() && maxdur)
    {
      if (auto resizer
          = m_context.app.interfaces<Scenario::IntervalResizerList>().make(
              m_itv, *maxdur))
        m_macro.submit(resizer);
    }
  }

  template <typename F>
  auto addProcess(F&& fun) -> decltype(auto)
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

bool DropProcessInScenario::drop(
    const ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  const auto& ctx = pres.context().context;
  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res).value_or(TimeVal{10000});

    DropProcessInScenarioHelper dropper(pres, pos, t);

    score::Dispatcher_T disp{dropper.macro()};
    for (const auto& proc : res)
    {
      auto p = dropper.addProcess(
          [&](Scenario::Command::Macro& m,
              const IntervalModel& itv) -> Process::ProcessModel* {
            return m.createProcessInNewSlot(
                itv, proc.creation.key, proc.creation.customData);
          },
          proc.duration ? *proc.duration : t);
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

bool DropProcessInInterval::drop(
    const score::DocumentContext& ctx,
    const IntervalModel& cst,
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
          [&](Scenario::Command::Macro& m,
              const IntervalModel& itv) -> Process::ProcessModel* {
            return m.createProcessInNewSlot(
                itv, proc.creation.key, proc.creation.customData);
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

bool DropPortInScenario::drop(
    const ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::port()))
  {
    auto base_port = Dataflow::PortItem::clickedPort;
    if (!base_port || base_port->port().type != Process::PortType::Message
        || qobject_cast<Process::Outlet*>(&base_port->port()))
      return false;

    auto port = dynamic_cast<Dataflow::AutomatablePortItem*>(base_port);
    if (!port)
      return false;

    Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro,
                               pres.context().context};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = std::chrono::seconds{5};

    auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

    // Create process
    auto ok = port->on_createAutomation(
        interval,
        [&](score::Command* cmd) { m.submit(cmd); },
        pres.context().context);
    if (!ok)
    {
      return false;
    }

    m.showRack(interval);

    m.commit();
    return true;
  }

  return false;
}

bool DropScenario::drop(
    const ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  if (mime.hasUrls())
  {
    const auto& doc = pres.context().context;
    auto& sm = pres.model();
    auto path = mime.urls().first().toLocalFile();
    if (QFile f{path};
        QFileInfo{f}.suffix() == "scenario" && f.open(QIODevice::ReadOnly))
    {
      CommandDispatcher<> d{doc.commandStack};
      d.submit(new Scenario::Command::ScenarioPasteElements(
          sm,
          QJsonDocument::fromJson(f.readAll()).object(),
          pres.toScenarioPoint(pos)));
      return true;
    }
  }

  return false;
}

bool DropLayerInScenario::drop(
    const ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  QJsonObject json;
  if (mime.formats().contains(score::mime::layerdata()))
  {
    json = QJsonDocument::fromJson(mime.data(score::mime::layerdata()))
               .object();
  }
  else if (mime.hasUrls())
  {
    if (QFile f{mime.urls()[0].toLocalFile()};
        QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
    {
      json = QJsonDocument::fromJson(f.readAll()).object();
    }
  }

  if (json.empty())
    return false;

  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro,
                             pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  const Scenario::Point pt = pres.toScenarioPoint(pos);

  const TimeVal t = TimeVal::fromMsecs(json["Duration"].toDouble());

  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  DropLayerInInterval::perform(interval, pres.context().context, m, json);
  m.commit();
  return true;
}

void DropLayerInInterval::perform(
    const IntervalModel& interval,
    const score::DocumentContext& ctx,
    Scenario::Command::Macro& m,
    const QJsonObject& json)
{
  const auto pid = ossia::get_pid();
  const bool same_doc
      = (pid == json["PID"].toInt())
        && (ctx.document.id().val() == json["Document"].toInt());
  const bool small_view = json["View"].toString() == "Small";
  const int slot_index = json["SlotIndex"].toInt();

  if (same_doc)
  {
    const auto old_p
        = fromJsonObject<Path<Process::ProcessModel>>(json["Path"]);
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
    auto cables = fromJsonValueArray<Dataflow::SerializedCables>(
        json["Cables"].toArray());

    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

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
    const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::layerdata()))
  {
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, ctx};

    const auto json
        = QJsonDocument::fromJson(mime.data(score::mime::layerdata()))
              .object();
    perform(interval, ctx, m, json);
    m.commit();
    return true;
  }
  else if (mime.hasUrls())
  {
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, ctx};
    bool ok = false;
    for (const QUrl& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if (QFile f{path};
          QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
      {
        ok = true;
        QJsonParseError e{};
        const auto json = QJsonDocument::fromJson(f.readAll(), &e).object();
        if (e.error == QJsonParseError::NoError)
          perform(interval, ctx, m, json);
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
    // FIXME see https://github.com/OSSIA/libossia/issues/291
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
  else if (mime.formats().contains(score::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{mime};

    CreateCurvesFromAddresses({&cst}, {des.deserialize()}, ctx.commandStack);
    return true;
  }
  else
  {
    return false;
  }
}
}
