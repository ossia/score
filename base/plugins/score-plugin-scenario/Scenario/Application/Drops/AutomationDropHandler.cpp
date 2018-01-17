// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationDropHandler.hpp"
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <ossia/network/value/value_traits.hpp>
#include <Dataflow/UI/PortItem.hpp>
namespace Scenario
{

bool DropProcessInScenario::drop(
    const TemporalScenarioPresenter& pres, QPointF pos, const QMimeData* mime)
{
  if (mime->formats().contains(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{*mime};
    Process::ProcessData p = des.deserialize();

    RedoMacroCommandDispatcher<Scenario::Command::AddProcessInNewBoxMacro> m{
        pres.context().context.commandStack};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = std::chrono::seconds{5};

    // Create the beginning
    auto start_cmd = new Scenario::Command::CreateTimeSync_Event_State{
        scenar, pt.date, pt.y};
    m.submitCommand(start_cmd);

    // Create a box with the duration of the longest song
    auto box_cmd
        = new Scenario::Command::CreateInterval_State_Event_TimeSync{
            scenar, start_cmd->createdState(), pt.date + t, pt.y};
    m.submitCommand(box_cmd);
    auto& interval = scenar.interval(box_cmd->createdInterval());

    // Create process
    auto process_cmd
        = new Scenario::Command::AddOnlyProcessToInterval{interval, p.key, p.customData};
    m.submitCommand(process_cmd);

    // Create a new slot
    auto slot_cmd = new Scenario::Command::AddSlotToRack{interval};
    m.submitCommand(slot_cmd);

    // Add a new layer in this slot.
    auto& proc = interval.processes.at(process_cmd->processId());
    auto layer_cmd = new Scenario::Command::AddLayerModelToSlot{
        SlotPath{interval, int(interval.smallView().size() - 1)},
        proc};

    m.submitCommand(layer_cmd);

    // Finally we show the newly created rack
    auto show_cmd = new Scenario::Command::ShowRack{interval};
    m.submitCommand(show_cmd);

    m.commit();
    return true;
  }

  return false;
}

bool DropPortInScenario::drop(
    const TemporalScenarioPresenter& pres, QPointF pos, const QMimeData* mime)
{
  if (mime->formats().contains(score::mime::port()))
  {
    auto port = Dataflow::PortItem::clickedPort;
    if(!port || port->port().type != Process::PortType::Message || qobject_cast<Process::Outlet*>(&port->port()))
      return false;

    RedoMacroCommandDispatcher<Scenario::Command::AddProcessInNewBoxMacro> m{
        pres.context().context.commandStack};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = std::chrono::seconds{5};

    // Create the beginning
    auto start_cmd = new Scenario::Command::CreateTimeSync_Event_State{
        scenar, pt.date, pt.y};
    m.submitCommand(start_cmd);

    // Create a box with the duration of the longest song
    auto box_cmd
        = new Scenario::Command::CreateInterval_State_Event_TimeSync{
            scenar, start_cmd->createdState(), pt.date + t, pt.y};
    m.submitCommand(box_cmd);
    auto& interval = scenar.interval(box_cmd->createdInterval());

    // Create process
    auto ok = port->on_createAutomation(interval, [&] (score::Command* cmd) {
              m.submitCommand(cmd);
    }, pres.context().context);
    if(!ok)
    {
      m.rollback();
      return false;
    }

    // Finally we show the newly created rack
    auto show_cmd = new Scenario::Command::ShowRack{interval};
    m.submitCommand(show_cmd);

    m.commit();
    return true;
  }

  return false;
}


bool DropProcessInInterval::drop(
    const IntervalModel& cst, const QMimeData* mime)
{
  if (mime->formats().contains(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{*mime};
    Process::ProcessData p = des.deserialize();

    auto& doc = score::IDocument::documentContext(cst);

    auto cmd = new Scenario::Command::AddProcessToInterval(cst, p.key, p.customData);
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
    if (ossia::is_numeric(addr.value)
        || addr.value.getType() == ossia::val_type::VEC2F
        || addr.value.getType() == ossia::val_type::VEC3F
        || addr.value.getType() == ossia::val_type::VEC4F )
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
    const IntervalModel& cst, const QMimeData* mime)
{
  // TODO refactor with AddressEditWidget
  if (mime->formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{*mime};
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
  else if (mime->formats().contains(score::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{*mime};
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
