#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
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

void CreateCurves(
        const QList<const ConstraintModel*>& selected_constraints,
        iscore::CommandStackFacade& stack)
{
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

    // Then create the commands
    auto big_macro = new AddMultipleProcessesToMultipleConstraintsMacro;
    if(!addresses.empty())
    {
        for(auto& constraint : selected_constraints)
        {
            // Generate brand new ids for the processes
            auto process_ids = getStrongIdRange<Process>(addresses.size(), constraint->processes);
            auto macro_tuple = makeAddProcessMacro(*constraint, addresses.size());
            auto macro = std::get<0>(macro_tuple);
            auto& bigLayerVec = std::get<1>(macro_tuple);

            Path<ConstraintModel> constraintPath{*constraint};
            int i = 0;
            for(const iscore::FullAddressSettings& as : addresses)
            {
                double min = as.domain.min.val.isNumeric()
                             ? iscore::convert::value<double>(as.domain.min)
                             : 0;

                double max = as.domain.max.val.isNumeric()
                             ? iscore::convert::value<double>(as.domain.max)
                             : 1;

                double start = std::min(min, max);
                double end = std::max(min, max);

                macro->addCommand(new CreateCurveFromStates{
                                      Path<ConstraintModel>{constraintPath},
                                      bigLayerVec[i],
                                      process_ids[i],
                                      as.address,
                                      start, end, min, max
                                  });

                i++;
            }
            big_macro->addCommand(macro);
        }
    }

    CommandDispatcher<> disp{stack};
    disp.submitCommand(big_macro);
}
