#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Process/State/MessageNode.hpp>
#include <QList>
#include <QPointer>

#include "CreateCurves.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/tools/IdentifiedObjectAbstract.hpp>

#include <Automation/AutomationModel.hpp>
void CreateCurves(
        const QList<const ConstraintModel*>& selected_constraints,
        iscore::CommandStackFacade& stack)
{
    if(selected_constraints.empty())
        return;

    // For each constraint, interpolate between the states in its start event and end event.

    // They should all be in the same scenario so we can select the first.
    Scenario::ScenarioModel* scenar = dynamic_cast<Scenario::ScenarioModel*>(
                                selected_constraints.first()->parent());

    auto& doc = iscore::IDocument::documentContext(*scenar);


    // First get the addresses
    auto& device_explorer = deviceExplorerFromContext(doc);

    std::vector<iscore::FullAddressSettings> addresses;
    for(const auto& index : device_explorer.selectedIndexes())
    {
        const auto& node = device_explorer.nodeFromModelIndex(index);
        if(node.is<iscore::AddressSettings>())
        {
            const iscore::AddressSettings& addr = node.get<iscore::AddressSettings>();
            if(addr.value.val.isNumeric())
            {
                iscore::FullAddressSettings as;
                static_cast<iscore::AddressSettingsCommon&>(as) = addr;
                as.address = iscore::address(node);
                addresses.push_back(std::move(as));
            }
        }
    }

    if(addresses.empty())
        return;

    int added_processes = 0;
    // Then create the commands
    auto big_macro = new AddMultipleProcessesToMultipleConstraintsMacro;

    for(const auto& constraint : selected_constraints)
    {
        // Generate brand new ids for the processes
        auto process_ids = getStrongIdRange<Process::ProcessModel>(addresses.size(), constraint->processes);
        auto macro_tuple = makeAddProcessMacro(*constraint, addresses.size());
        auto macro = std::get<0>(macro_tuple);
        auto& bigLayerVec = std::get<1>(macro_tuple);

        Path<ConstraintModel> constraintPath{*constraint};
        const StateModel& ss = startState(*constraint, *scenar);
        const auto& es = endState(*constraint, *scenar);

        std::vector<iscore::Address> existing_automations;
        for(const auto& proc : constraint->processes)
        {
            if(auto autom = dynamic_cast<const Automation::ProcessModel*>(&proc))
                existing_automations.push_back(autom->address());
        }

        int i = 0;
        for(const iscore::FullAddressSettings& as : addresses)
        {
            // First, we skip the curve if there is already a curve
            // with this address in the constraint.
            if(contains(existing_automations, as.address))
                continue;

            // Then we set-up all the necessary values
            // min / max
            double min = as.domain.min.val.isNumeric()
                    ? iscore::convert::value<double>(as.domain.min)
                    : 0;

            double max = as.domain.max.val.isNumeric()
                    ? iscore::convert::value<double>(as.domain.max)
                    : 1;

            // start value / end value
            double start = std::min(min, max);
            double end = std::max(min, max);
            MessageNode* s_node = iscore::try_getNodeFromString(
                        ss.messages().rootNode(),
                        stringList(as.address));
            if(s_node)
            {
                if(auto val = s_node->value())
                {
                    start = iscore::convert::value<double>(*val);
                    min = std::min(start, min);
                    max = std::max(start, max);
                }
            }

            MessageNode* e_node = iscore::try_getNodeFromString(
                        es.messages().rootNode(),
                        stringList(as.address));
            if(e_node)
            {
                if(auto val = e_node->value())
                {
                    end = iscore::convert::value<double>(*val);
                    min = std::min(end, min);
                    max = std::max(end, max);
                }
            }

            // Send the command.
            macro->addCommand(new CreateCurveFromStates{
                                  Path<ConstraintModel>{constraintPath},
                                  bigLayerVec[i],
                                  process_ids[i],
                                  as.address,
                                  start, end, min, max
                              });

            i++;
            added_processes++;
        }
        big_macro->addCommand(macro);
    }

    if(added_processes > 0)
    {
        CommandDispatcher<> disp{stack};
        disp.submitCommand(big_macro);
    }
    else
    {
        delete big_macro;
    }
}
