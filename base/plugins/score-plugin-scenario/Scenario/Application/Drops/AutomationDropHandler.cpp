// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationDropHandler.hpp"

#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <score/document/DocumentContext.hpp>
#include <ossia/network/value/value_traits.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <ossia/detail/thread.hpp>
#include <QFileInfo>
namespace Scenario
{

bool DropProcessInScenario::drop(
    const TemporalScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{mime};
    Process::ProcessData p = des.deserialize();

    Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = std::chrono::seconds{5};

    auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

    // Create process
    auto proc = m.createProcess(interval, p.key, p.customData);

    // Create a new slot
    m.createSlot(interval);

    // Add a new layer in this slot.
    m.addLayerToLastSlot(interval, *proc);

    // Finally we show the newly created rack
    m.showRack(interval);

    m.commit();
    return true;
  }

  return false;
}

bool DropPortInScenario::drop(
    const TemporalScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
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

    Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = std::chrono::seconds{5};

    auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

    // Create process
    auto ok = port->on_createAutomation(
        interval, [&](score::Command* cmd) { m.submit(cmd); },
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

bool DropLayerInScenario::drop(
    const TemporalScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::layerdata()))
  {
    Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, pres.context().context};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    const Scenario::Point pt = pres.toScenarioPoint(pos);

    const auto json = QJsonDocument::fromJson(mime.data(score::mime::layerdata())).object();

    const TimeVal t = TimeVal::fromMsecs(json["Duration"].toDouble());

    auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

    const auto& doc = score::IDocument::documentContext(scenar);
    DropLayerInInterval::perform(interval, doc, m, json);
    m.commit();
    return true;
  }

  return false;
}

void DropLayerInInterval::perform(
    const IntervalModel& interval, const score::DocumentContext& doc, Scenario::Command::Macro& m, const QJsonObject& json)
{
  const auto pid = ossia::get_pid();
  const bool same_doc = (pid == json["PID"].toInt()) && (doc.document.id().val() == json["Document"] .toInt());
  const bool small_view = json["View"].toString() == "Small";
  const int slot_index = json["SlotIndex"].toInt();

  const auto old_p = fromJsonObject<Path<Process::ProcessModel>>(json["Path"]);
  if(same_doc)
  {
    if(auto obj = old_p.try_find(doc))
    if(auto itv = qobject_cast<IntervalModel*>(obj->parent()))
    {
      if(small_view && (qApp->keyboardModifiers() & Qt::ALT))
      {
        m.moveSlot(*itv, interval, slot_index);
      }
      else
      {
        m.moveProcess(*itv, interval, obj->id());
      }
    }
  }
  else
  {
    // Just create a new process
    m.loadProcessInSlot(interval, json);
  }

  // Finally we show the newly created rack
  m.showRack(interval);
}

bool DropLayerInInterval::drop(
    const IntervalModel& interval, const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::layerdata()))
  {
    const auto& doc = score::IDocument::documentContext(interval);
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc};

    const auto json = QJsonDocument::fromJson(mime.data(score::mime::layerdata())).object();
    perform(interval, doc, m, json);
    m.commit();
    return true;
  }
  else if(mime.hasUrls())
  {
    const auto& doc = score::IDocument::documentContext(interval);
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, doc};
    bool ok = false;
    for(const QUrl& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if(QFile f{path}; QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
      {
        ok = true;
        QJsonParseError e{};
        const auto json = QJsonDocument::fromJson(f.readAll(), &e).object();
        if(e.error == QJsonParseError::NoError)
          perform(interval, doc, m, json);
      }
    }

    if(ok)
      m.commit();
    return true;
  }

  return false;
}

bool DropProcessInInterval::drop(
    const IntervalModel& cst, const QMimeData& mime)
{
  if (mime.formats().contains(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{mime};
    Process::ProcessData p = des.deserialize();

    auto& doc = score::IDocument::documentContext(cst);

    auto cmd = new Scenario::Command::AddProcessToInterval(
        cst, p.key, p.customData);
    CommandDispatcher<> d{doc.commandStack};
    d.submitCommand(cmd);
    return true;
  }
  else
  {
    return false;
  }
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
    const IntervalModel& cst, const QMimeData& mime)
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

    auto& doc = score::IDocument::documentContext(cst);
    CreateCurvesFromAddresses({&cst}, addresses, doc.commandStack);

    return true;
  }
  else if (mime.formats().contains(score::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{mime};
    auto& doc = score::IDocument::documentContext(cst);

    CreateCurvesFromAddresses({&cst}, {des.deserialize()}, doc.commandStack);
    return true;
  }
  else
  {
    return false;
  }
}
}
