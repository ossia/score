// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateSequence.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <State/Address.hpp>
#include <State/Domain.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_variant_visitors.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QByteArray>
#include <QList>

#include <list>
#include <utility>
#include <vector>
namespace Scenario
{
namespace Command
{

struct color_converter
{
  template <typename Color>
  QColor operator()(const typename Color::value_type& value, const Color&)
  {
    auto rgba = ossia::rgba{ossia::strong_value<Color>{value}};
    auto& col = rgba.dataspace_value;
    return QColor::fromRgbF((qreal)col[0], (qreal)col[1], (qreal)col[2], (qreal)col[3]);
  }

  template <typename... Args>
  QColor operator()(Args&&...)
  {
    return QColor{};
  }
};
CreateSequenceProcesses::CreateSequenceProcesses(
    const Scenario::ProcessModel& scenario,
    const Scenario::IntervalModel& interval)
    : m_scenario{scenario}, m_endState{Scenario::endState(interval, scenario).id()}
{
  // TESTME

  if (!score::AppContext().settings<Scenario::Settings::Model>().getAutoSequence())
    return;

  // We get the device explorer, and we fetch the new states.
  const auto& startMessages
      = Process::flatten(Scenario::startState(interval, scenario).messages().rootNode());

  std::vector<Device::FullAddressSettings> endAddresses;
  endAddresses.reserve(startMessages.size());
  ossia::transform(startMessages, std::back_inserter(endAddresses), [](const auto& mess) {
    return Device::FullAddressSettings::make(mess);
  });

  auto& devPlugin
      = score::IDocument::documentContext(scenario).plugin<Explorer::DeviceDocumentPlugin>();
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
  ossia::transform(endAddresses, std::back_inserter(endMessages), [](const auto& addr) {
    auto m = State::Message{State::AddressAccessor{addr.address}, addr.value};
    m.address.qualifiers.get().unit = addr.unit;
    return m;
  });

  updateTreeWithMessageList(m_stateData, endMessages);

  // We also create relevant curves.
  std::vector<std::pair<State::Message, Device::FullAddressSettings>> matchingNumericMessages;
  std::vector<std::pair<State::Message, Device::FullAddressSettings>> matchingListMessages;
  std::vector<std::pair<State::Message, Device::FullAddressSettings>> matchingColorMessages;
  // First we filter the messages
  for (auto& message : startMessages)
  {
    if (ossia::is_numeric(message.value))
    {
      auto addr_it = ossia::find_if(endAddresses, [&](const Device::FullAddressSettings& arg) {
        return message.address.address == arg.address && message.value != arg.value;
      });

      if (addr_it != std::end(endAddresses))
      {
        matchingNumericMessages.emplace_back(message, *addr_it);
      }
    }
    else if (ossia::is_array(message.value))
    {
      auto addr_it = ossia::find_if(endAddresses, [&](const Device::FullAddressSettings& arg) {
        return message.address.address == arg.address && message.value != arg.value;
      });

      if (addr_it != std::end(endAddresses))
      {
        const auto& unit = message.address.qualifiers.get().unit;
        if (message.address.qualifiers.get().unit.v.target<ossia::color_u>())
        {
          matchingColorMessages.emplace_back(message, *addr_it);
        }
        else if (unit.which() == ossia::unit_variant::npos)
        {
          // Due to bugs we disable autosequences with array units
          // TODO handle sub-vecs
          auto sz = message.value.apply(value_size{});
          for (std::size_t i = 0; i < sz; i++)
          {
            auto m = message;
            auto& acc = m.address.qualifiers.get().accessors;
            acc.clear();
            acc.push_back(sz - i - 1);
            matchingListMessages.emplace_back(m, *addr_it);
          }
        }
      }
    }
  }

  // Then, if there are correct messages we can actually do our interpolation.
  m_addedProcessCount = matchingNumericMessages.size() + matchingListMessages.size()
                        + matchingColorMessages.size();
  if (m_addedProcessCount == 0)
    return;

  {
    AddMultipleProcessesToIntervalMacro interpolateMacro{interval};
    m_interpolations.slotsToUse = interpolateMacro.slotsToUse;
    m_interpolations.commands() = interpolateMacro.takeCommands();
  }

  // Generate brand new ids for the processes
  auto process_ids = getStrongIdRange<Process::ProcessModel>(m_addedProcessCount);

  int cur_proc = 0;
  // Here we know that there is nothing yet, so we can just assign
  // ids 1, 2, 3, 4 to each process and each process view in each slot
  for (const auto& elt : matchingNumericMessages)
  {
    auto start = State::convert::value<double>(elt.first.value);
    auto end = State::convert::value<double>(elt.second.value);
    Curve::CurveDomain d{elt.second.domain.get(), start, end};
    auto cmd = new CreateAutomationFromStates{
        interval, m_interpolations.slotsToUse, process_ids[cur_proc], elt.first.address, d};
    m_interpolations.addCommand(cmd);
    cur_proc++;
  }

  for (const auto& elt : matchingListMessages)
  {
    const auto& idx = elt.first.address.qualifiers.get().accessors;
    Curve::CurveDomain d = ossia::apply(
        get_curve_domain{elt.first.address, idx, rootNode}, elt.first.value.v, elt.second.value.v);

    m_interpolations.addCommand(new CreateAutomationFromStates{
        interval, m_interpolations.slotsToUse, process_ids[cur_proc], elt.first.address, d});
    cur_proc++;
  }

  for (const auto& elt : matchingColorMessages)
  {
    const auto& start_qual = elt.first.address.qualifiers.get();
    auto start_color = start_qual.unit.v.target<ossia::color_u>();
    if (!start_color)
      continue;
    auto end_color = elt.second.unit.get().v.target<ossia::color_u>();
    if (!end_color)
      continue;

    QColor start = ossia::apply(color_converter{}, elt.first.value.v, *start_color);
    QColor end = ossia::apply(color_converter{}, elt.second.value.v, *end_color);

    m_interpolations.addCommand(new CreateGradient{
        interval,
        m_interpolations.slotsToUse,
        process_ids[cur_proc],
        elt.first.address,
        start,
        end});
    cur_proc++;
  }
}

void CreateSequenceProcesses::undo(const score::DocumentContext& ctx) const
{
  if (m_addedProcessCount > 0)
    m_interpolations.undo(ctx);
}

void CreateSequenceProcesses::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenario.find(ctx);
  auto& endstate = scenar.state(m_endState);

  endstate.messages() = m_stateData;

  if (m_addedProcessCount > 0)
    m_interpolations.redo(ctx);
}

void CreateSequenceProcesses::serializeImpl(DataStreamInput& s) const
{
  s << m_scenario << m_interpolations.serialize() << m_stateData << m_endState
    << m_addedProcessCount;
}

void CreateSequenceProcesses::deserializeImpl(DataStreamOutput& s)
{
  QByteArray interp;
  s >> m_scenario >> interp >> m_stateData >> m_endState >> m_addedProcessCount;
  m_interpolations.deserialize(interp);
}

CreateSequence* CreateSequence::make(
    const score::DocumentContext& ctx,
    const ProcessModel& scenario,
    const Id<StateModel>& start,
    const TimeVal& date,
    double endStateY)
{
  auto cmd = new CreateSequence;

  auto create_command
      = new CreateInterval_State_Event_TimeSync{scenario, start, date, endStateY, false};
  cmd->m_newInterval = create_command->createdInterval();
  cmd->m_newState = create_command->createdState();
  cmd->m_newEvent = create_command->createdEvent();
  cmd->m_newTimeSync = create_command->createdTimeSync();

  create_command->redo(ctx);
  cmd->addCommand(create_command);

  auto proc_command = new CreateSequenceProcesses{
      scenario, scenario.interval(create_command->createdInterval())};

  if (proc_command->addedProcessCount() > 0)
  {
    proc_command->redo(ctx);
    cmd->addCommand(proc_command);

    auto show_rack = new ShowRack{scenario.interval(create_command->createdInterval())};
    show_rack->redo(ctx);
    cmd->addCommand(show_rack);
  }
  else
  {
    delete proc_command;
  }
  return cmd;
}
}
}
