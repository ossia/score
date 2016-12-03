#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>
#include <algorithm>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iterator>
#include <list>
#include <utility>
#include <vector>

#include "CreateSequence.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/TreeNode.hpp>

#include <ossia/editor/value/value_conversion.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <ossia/network/domain/domain.hpp>
namespace Scenario
{
namespace Command
{

CreateSequenceProcesses::CreateSequenceProcesses(
    const Scenario::ProcessModel& scenario,
    const Scenario::ConstraintModel& constraint)
    : m_scenario{scenario}
    , m_endState{Scenario::endState(constraint, scenario).id()}
{
  // TESTME

  if (!context.settings<Scenario::Settings::Model>().getAutoSequence())
    return;

  // We get the device explorer, and we fetch the new states.
  const auto& startMessages = Process::flatten(
      Scenario::startState(constraint, scenario).messages().rootNode());

  std::vector<Device::FullAddressSettings> endAddresses;
  endAddresses.reserve(startMessages.size());
  ossia::transform(
      startMessages, std::back_inserter(endAddresses), [](const auto& mess) {
        return Device::FullAddressSettings::make(mess);
      });

  auto& devPlugin = iscore::IDocument::documentContext(scenario)
                        .plugin<Explorer::DeviceDocumentPlugin>();
  auto& rootNode = devPlugin.rootNode();

  for (auto it = endAddresses.begin(); it != endAddresses.end();)
  {
    auto& mess = *it;

    auto node = Device::try_getNodeFromAddress(rootNode, mess.address);

    if (node && node->is<Device::AddressSettings>())
    {
      // TODO this would be a nice use of futures
      devPlugin.updateProxy.refreshRemoteValue(mess.address);
      const auto& nodeImpl = node->get<Device::AddressSettings>();
      static_cast<Device::AddressSettingsCommon&>(mess)
          = static_cast<const Device::AddressSettingsCommon&>(nodeImpl);
      ++it;
    }
    else
    {
      it = endAddresses.erase(it);
    }
  }

  QList<State::Message> endMessages;
  endMessages.reserve(endAddresses.size());
  ossia::transform(
      endAddresses, std::back_inserter(endMessages), [](const auto& addr) {
        return State::Message{State::AddressAccessor{addr.address},
                              addr.value};
      });

  updateTreeWithMessageList(m_stateData, endMessages);

  // We also create relevant curves.
  std::vector<std::pair<State::Message, Device::FullAddressSettings>>
      matchingNumericMessages;
  std::vector<std::pair<State::Message, Device::FullAddressSettings>>
      matchingTupleMessages;
  // First we filter the messages
  for (auto& message : startMessages)
  {
    if (message.value.val.isNumeric())
    {
      auto addr_it = ossia::find_if(
          endAddresses, [&](const Device::FullAddressSettings& arg) {
            return message.address.address == arg.address
                   && message.value != arg.value;
          });

      if (addr_it != std::end(endAddresses))
      {
        matchingNumericMessages.emplace_back(message, *addr_it);
      }
    }
    else if (message.value.val.isArray())
    {
      auto addr_it = ossia::find_if(
          endAddresses, [&](const Device::FullAddressSettings& arg) {
            return message.address.address == arg.address
                   && message.value != arg.value;
          });

      if (addr_it != std::end(endAddresses))
      {
        matchingTupleMessages.emplace_back(message, *addr_it);
      }
    }
  }

  // Then, if there are correct messages we can actually do our interpolation.
  m_addedProcessCount
      = matchingNumericMessages.size() + matchingTupleMessages.size();
  if (m_addedProcessCount == 0)
    return;

  {
    AddMultipleProcessesToConstraintMacro interpolateMacro{constraint};
    m_interpolations.slotsToUse = interpolateMacro.slotsToUse;
    m_interpolations.commands() = interpolateMacro.takeCommands();
  }

  // Generate brand new ids for the processes
  auto process_ids
      = getStrongIdRange<Process::ProcessModel>(m_addedProcessCount);
  auto layers_ids = getStrongIdRange<Process::LayerModel>(m_addedProcessCount);

  int cur_proc = 0;
  // Here we know that there is nothing yet, so we can just assign
  // ids 1, 2, 3, 4 to each process and each process view in each slot
  for (const auto& elt : matchingNumericMessages)
  {
    std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>
        layer_vect;
    for (const auto& slots_elt : m_interpolations.slotsToUse)
    {
      layer_vect.push_back(
          std::make_pair(slots_elt.first, layers_ids[cur_proc]));
    }

    auto start = State::convert::value<double>(elt.first.value);
    auto end = State::convert::value<double>(elt.second.value);
    auto& dom = elt.second.domain.get();
    auto min_v = dom.get_min();
    auto max_v = dom.get_max();
    double min
        = (min_v.valid())
              ? std::min(ossia::convert<double>(min_v), std::min(start, end))
              : std::min(start, end);
    double max
        = (max_v.valid())
              ? std::max(ossia::convert<double>(max_v), std::max(start, end))
              : std::max(start, end);

    auto cmd = new CreateAutomationFromStates{constraint,
                                              layer_vect,
                                              process_ids[cur_proc],
                                              elt.first.address,
                                              start,
                                              end,
                                              min,
                                              max};
    m_interpolations.addCommand(cmd);
    cur_proc++;
  }

  for (const auto& elt : matchingTupleMessages)
  {
    std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>
        layer_vect;
    for (const auto& slots_elt : m_interpolations.slotsToUse)
    {
      layer_vect.push_back(
          std::make_pair(slots_elt.first, layers_ids[cur_proc]));
    }

    m_interpolations.addCommand(new CreateInterpolationFromStates{
        constraint, layer_vect, process_ids[cur_proc], elt.first.address,
        elt.first.value, elt.second.value});
    cur_proc++;
  }
}

void CreateSequenceProcesses::undo() const
{
  if (m_addedProcessCount > 0)
    m_interpolations.undo();
}

void CreateSequenceProcesses::redo() const
{
  auto& scenar = m_scenario.find();
  auto& endstate = scenar.state(m_endState);

  endstate.messages() = m_stateData;

  if (m_addedProcessCount > 0)
    m_interpolations.redo();
}

void CreateSequenceProcesses::serializeImpl(DataStreamInput& s) const
{
  s << m_scenario << m_interpolations.serialize() << m_stateData << m_endState
    << m_addedProcessCount;
}

void CreateSequenceProcesses::deserializeImpl(DataStreamOutput& s)
{
  QByteArray interp;
  s >> m_scenario >> interp >> m_stateData >> m_endState
      >> m_addedProcessCount;
  m_interpolations.deserialize(interp);
}

CreateSequence* CreateSequence::make(
    const ProcessModel& scenario,
    const Id<StateModel>& start,
    const TimeValue& date,
    double endStateY)
{
  auto cmd = new CreateSequence;

  auto create_command = new CreateConstraint_State_Event_TimeNode{
      scenario, start, date, endStateY};
  cmd->m_newConstraint = create_command->createdConstraint();
  cmd->m_newState = create_command->createdState();
  cmd->m_newEvent = create_command->createdEvent();
  cmd->m_newTimeNode = create_command->createdTimeNode();

  create_command->redo();
  cmd->addCommand(create_command);

  auto proc_command = new CreateSequenceProcesses{
      scenario, scenario.constraint(create_command->createdConstraint())};
  proc_command->redo();
  cmd->addCommand(proc_command);

  return cmd;
}
}
}
