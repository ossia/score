// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationDropHandler.hpp"
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <ossia/editor/value/value_traits.hpp>
namespace Scenario
{

bool DropProcessInScenario::drop(
    const TemporalScenarioPresenter& pres, QPointF pos, const QMimeData* mime)
{
  if (mime->formats().contains(iscore::mime::processdata()))
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
    auto start_cmd = new Scenario::Command::CreateTimeNode_Event_State{
        scenar, pt.date, pt.y};
    m.submitCommand(start_cmd);

    // Create a box with the duration of the longest song
    auto box_cmd
        = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
            scenar, start_cmd->createdState(), pt.date + t, pt.y};
    m.submitCommand(box_cmd);
    auto& constraint = scenar.constraint(box_cmd->createdConstraint());

    // Create process
    auto process_cmd
        = new Scenario::Command::AddOnlyProcessToConstraint{constraint, p.key};
    m.submitCommand(process_cmd);

    // Create a new slot
    auto slot_cmd = new Scenario::Command::AddSlotToRack{constraint};
    m.submitCommand(slot_cmd);

    // Add a new layer in this slot.
    auto& proc = constraint.processes.at(process_cmd->processId());
    auto layer_cmd = new Scenario::Command::AddLayerModelToSlot{
        SlotPath{constraint, int(constraint.smallView().size() - 1)},
        proc};

    m.submitCommand(layer_cmd);

    // Finally we show the newly created rack
    auto show_cmd = new Scenario::Command::ShowRack{constraint};
    m.submitCommand(show_cmd);

    m.commit();
    return true;
  }
  else
  {
    return false;
  }

  return false;
}

bool DropProcessInConstraint::drop(
    const ConstraintModel& cst, const QMimeData* mime)
{
  if (mime->formats().contains(iscore::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{*mime};
    Process::ProcessData p = des.deserialize();

    auto& doc = iscore::IDocument::documentContext(cst);

    auto cmd = Scenario::Command::make_AddProcessToConstraint(cst, p.key);
    if (cmd)
    {
      CommandDispatcher<> d{doc.commandStack};
      d.submitCommand(cmd);
    }
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
    if (ossia::is_numeric(addr.value))
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
    const ConstraintModel& cst, const QMimeData* mime)
{
  // TODO refactor with AddressEditWidget
  if (mime->formats().contains(iscore::mime::nodelist()))
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

    auto& doc = iscore::IDocument::documentContext(cst);
    CreateCurvesFromAddresses({&cst}, addresses, doc.commandStack);

    return true;
  }
  else if (mime->formats().contains(iscore::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{*mime};
    auto& doc = iscore::IDocument::documentContext(cst);

    CreateCurvesFromAddresses({&cst}, {des.deserialize()}, doc.commandStack);
    return true;
  }
  else
  {
    return false;
  }
}
}
