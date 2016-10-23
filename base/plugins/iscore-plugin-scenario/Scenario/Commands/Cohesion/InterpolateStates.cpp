#include <Automation/AutomationModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QObject>
#include <QString>
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <Device/Address/Domain.hpp>
#include <Device/Node/DeviceNode.hpp>
#include "InterpolateStates.hpp"
#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/EntityMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <ossia/editor/value/value_conversion.hpp>

namespace Scenario
{
namespace Command
{
struct MessagePairs
{
        MessagePairs(const Scenario::ConstraintModel& constraint, const Scenario::ScenarioInterface& scenar):
            MessagePairs{
                Process::flatten(Scenario::startState(constraint, scenar).messages().rootNode()),
                Process::flatten(Scenario::endState(constraint, scenar).messages().rootNode()),
                constraint}
        {

        }

        MessagePairs(
                const State::MessageList& startMessages,
                const State::MessageList& endMessages,
                const Scenario::ConstraintModel& constraint)
        {
            for(auto& message : startMessages)
            {
                // First check if we can build a process from this
                if(message.value.val.isNumeric())
                {
                    // Look for a corresponding message on the end state
                    auto it = ossia::find_if(endMessages, [&] (const State::Message& arg) {
                        return message.address == arg.address
                                && arg.value.val.isNumeric()
                                && message.value != arg.value; });

                    if(it != endMessages.end())
                    {
                        // Check that there isn't already an automation with this address
                        auto has_existing_curve = ossia::any_of(constraint.processes,
                                    [&] (const Process::ProcessModel& proc) {
                            auto ptr = dynamic_cast<const Automation::ProcessModel*>(&proc);
                            return ptr && ptr->address() == message.address;
                        });

                        if(has_existing_curve)
                            continue;

                        // We can add this
                        numericMessages.emplace_back(message, *it);
                    }
                }
                else if(message.value.val.is<State::tuple_t>())
                {
                    auto it = ossia::find_if(endMessages, [&] (const State::Message& arg) {
                        return message.address == arg.address
                                && arg.value.val.is<State::tuple_t>()
                                && message.value != arg.value; });

                    if(it != endMessages.end())
                    {
                        // Check that there isn't already an interpolation with this address
                        auto has_existing_curve = ossia::any_of(constraint.processes,
                                    [&] (const Process::ProcessModel& proc) {
                            auto ptr = dynamic_cast<const Interpolation::ProcessModel*>(&proc);
                            return ptr && ptr->address() == message.address;
                        });

                        if(has_existing_curve)
                            continue;

                        // We can add this
                        tupleMessages.emplace_back(message, *it);
                    }
                }
            }
        }

        using messages_pairs = std::vector<std::pair<State::Message, State::Message>>;
        messages_pairs numericMessages;
        messages_pairs tupleMessages;

};

void InterpolateStates(const QList<const ConstraintModel*>& selected_constraints,
                       const iscore::CommandStackFacade& stack)
{
    // For each constraint, interpolate between the states in its start event and end event.
    if(selected_constraints.empty())
        return;

    // They should all be in the same scenario so we can select the first.
    auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(selected_constraints.first()->parent());
    if(!scenar)
        return;

    auto& devPlugin = iscore::IDocument::documentContext(*selected_constraints.first()).plugin<Explorer::DeviceDocumentPlugin>();
    auto& rootNode = devPlugin.rootNode();

    auto big_macro = std::make_unique<Command::AddMultipleProcessesToMultipleConstraintsMacro>();
    for(auto& constraint_ptr : selected_constraints)
    {
        auto& constraint = *constraint_ptr;
        // Find the matching pairs of messages from both sides of the constraint
        MessagePairs pairs{constraint, *scenar};

        int total_procs = pairs.numericMessages.size() + pairs.tupleMessages.size();
        if(total_procs == 0)
            continue;

        // Generate brand new ids for the processes, as well as layers, etc.
        auto process_ids = getStrongIdRange<Process::ProcessModel>(total_procs, constraint.processes);

        // Note : a *lot* of thins happen in makeAddProcessMacro.
        auto macro_tuple = Command::makeAddProcessMacro(constraint, total_procs);

        // TODO Refactor with structured bindings when C++17
        auto macro = std::get<0>(macro_tuple);
        auto& bigLayerVec = std::get<1>(macro_tuple);

        int cur_proc = 0;
        // Generate automations between numeric values
        for(const auto& elt : pairs.numericMessages)
        {
            double start = State::convert::value<double>(elt.first.value);
            double end = State::convert::value<double>(elt.second.value);

            double min = std::min(start, end);
            double max = std::max(start, end);
            if(auto node = Device::try_getNodeFromAddress(rootNode, elt.first.address.address))
            {
                const Device::AddressSettings& as = node->get<Device::AddressSettings>();

                auto min_v = ossia::net::get_min(as.domain);
                auto max_v = ossia::net::get_max(as.domain);

                if(ossia::is_numeric(min_v))
                    min = std::min(min, ossia::convert<double>(min_v));

                if(ossia::is_numeric(max_v))
                    max = std::max(max, ossia::convert<double>(max_v));
            }

            macro->addCommand(new CreateAutomationFromStates{
                                  constraint,
                                  bigLayerVec[cur_proc],
                                  process_ids[cur_proc],
                                  elt.first.address,
                                  start, end, min, max
                              });

            cur_proc++;
        }

        // Generate interpolations between tuples
        for(const auto& elt : pairs.tupleMessages)
        {
            macro->addCommand(new CreateInterpolationFromStates{
                                  constraint,
                                  bigLayerVec[cur_proc],
                                  process_ids[cur_proc],
                                  elt.first.address,
                                  elt.first.value,
                                  elt.second.value
                              });
            cur_proc++;
        }

        big_macro->addCommand(macro);
    }

    if(!big_macro->commands().empty())
    {
        CommandDispatcher<> disp{stack};
        disp.submitCommand(big_macro.release());
    }
}
}
}
