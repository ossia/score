// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Automation/AutomationModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QObject>
#include <QString>
#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <algorithm>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <iterator>
#include <utility>
#include <vector>

#include "InterpolateStates.hpp"
#include <ossia/network/value/value_conversion.hpp>
#include <ossia/network/domain/domain.hpp>
#include <State/Domain.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/tree/TreeNode.hpp>

namespace Scenario
{
namespace Command
{
struct MessagePairs
{
  MessagePairs(
      const Scenario::IntervalModel& interval,
      const Scenario::ScenarioInterface& scenar)
      : MessagePairs{
            Process::flatten(Scenario::startState(interval, scenar)
                                 .messages()
                                 .rootNode()),
            Process::flatten(
                Scenario::endState(interval, scenar).messages().rootNode()),
            interval}
  {
  }

  MessagePairs(
      const State::MessageList& startMessages,
      const State::MessageList& endMessages,
      const Scenario::IntervalModel& interval)
  {
    for (auto& message : startMessages)
    {
      // First check if we can build a process from this
      if (ossia::is_numeric(message.value))
      {
        // Look for a corresponding message on the end state
        auto it = ossia::find_if(endMessages, [&](const State::Message& arg) {
          return message.address == arg.address && ossia::is_numeric(arg.value)
                 && message.value != arg.value;
        });

        if (it != endMessages.end())
        {
          // Check that there isn't already an automation with this address
          auto has_existing_curve = ossia::any_of(
              interval.processes, [&](const Process::ProcessModel& proc) {
                auto ptr
                    = dynamic_cast<const Automation::ProcessModel*>(&proc);
                return ptr && ptr->address() == message.address;
              });

          if (has_existing_curve)
            continue;

          // We can add this
          numericMessages.emplace_back(message, *it);
        }
      }
      else if (ossia::is_array(message.value))
      {
        auto it = ossia::find_if(endMessages, [&](const State::Message& arg) {
          return message.address == arg.address
                 && arg.value.v.which() == message.value.v.which()
                 && message.value != arg.value;
        });

        if (it != endMessages.end())
        {
          // Check that there isn't already an interpolation with this address
          auto has_existing_curve = ossia::any_of(
              interval.processes, [&](const Process::ProcessModel& proc) {
                auto ptr
                    = dynamic_cast<const Interpolation::ProcessModel*>(&proc);
                return ptr && ptr->address() == message.address;
              });

          if (has_existing_curve)
            continue;

          // We can add this
          listMessages.emplace_back(message, *it);
        }
      }
    }
  }

  using messages_pairs
      = std::vector<std::pair<State::Message, State::Message>>;
  messages_pairs numericMessages;
  messages_pairs listMessages;
};

void InterpolateStates(
    const QList<const IntervalModel*>& selected_intervals,
    const score::CommandStackFacade& stack)
{
  // For each interval, interpolate between the states in its start event and
  // end event.
  if (selected_intervals.empty())
    return;

  // They should all be in the same scenario so we can select the first.
  auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(
      selected_intervals.first()->parent());
  if (!scenar)
    return;

  auto& devPlugin
      = score::IDocument::documentContext(*selected_intervals.first())
            .plugin<Explorer::DeviceDocumentPlugin>();
  auto& rootNode = devPlugin.rootNode();

  auto big_macro = std::
      make_unique<Command::AddMultipleProcessesToMultipleIntervalsMacro>();
  for (auto& interval_ptr : selected_intervals)
  {
    auto& interval = *interval_ptr;
    // Find the matching pairs of messages from both sides of the interval
    MessagePairs pairs{interval, *scenar};

    int total_procs
        = pairs.numericMessages.size() + pairs.listMessages.size();
    if (total_procs == 0)
      continue;

    // Generate brand new ids for the processes, as well as layers, etc.
    auto process_ids = getStrongIdRange<Process::ProcessModel>(
        total_procs, interval.processes);

    // Note : a *lot* of thins happen in makeAddProcessMacro.
    auto macro = Command::makeAddProcessMacro(interval, total_procs);

    int cur_proc = 0;
    // Generate automations between numeric values
    for (const auto& elt : pairs.numericMessages)
    {
      double start = State::convert::value<double>(elt.first.value);
      double end = State::convert::value<double>(elt.second.value);

      Curve::CurveDomain d{start, end};

      if (auto node = Device::try_getNodeFromAddress(
              rootNode, elt.first.address.address))
      {
        const Device::AddressSettings& as
            = node->get<Device::AddressSettings>();

        d.refine(as.domain.get());
      }

      macro->addCommand(new CreateAutomationFromStates{
          interval, macro->slotsToUse, process_ids[cur_proc],
          elt.first.address, d});

      cur_proc++;
    }

    // Generate interpolations between lists
    for (const auto& elt : pairs.listMessages)
    {
      macro->addCommand(new CreateInterpolationFromStates{
          interval, macro->slotsToUse, process_ids[cur_proc],
          elt.first.address, elt.first.value, elt.second.value});
      cur_proc++;
    }

    big_macro->addCommand(macro);
  }

  if (!big_macro->commands().empty())
  {
    CommandDispatcher<> disp{stack};
    disp.submitCommand(big_macro.release());
  }
}
}
}
