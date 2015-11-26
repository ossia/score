#include "InterpolateStates.hpp"

#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>

#include <Automation/AutomationModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <Device/Address/AddressSettings.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

void InterpolateStates(iscore::Document* doc)
{
    using namespace std;

    // Fetch the selected constraints
    auto selected_constraints = filterSelectionByType<ConstraintModel>(doc->selectionStack().currentSelection());

    if(selected_constraints.empty())
        return;

    InterpolateStates(selected_constraints, doc->commandStack());
}

void InterpolateStates(const QList<const ConstraintModel*>& selected_constraints,
                       iscore::CommandStack& stack)
{
    // For each constraint, interpolate between the states in its start event and end event.

    // They should all be in the same scenario so we can select the first.
    Scenario::ScenarioModel* scenar = dynamic_cast<Scenario::ScenarioModel*>(
                                selected_constraints.first()->parent());

    auto& devPlugin = *iscore::IDocument::documentFromObject(*scenar)->model().pluginModel<DeviceDocumentPlugin>();
    auto& rootNode = devPlugin.rootNode();

    auto big_macro = new GenericInterpolateMacro;
    for(auto& constraint : selected_constraints)
    {
        const auto& startState = scenar->state(constraint->startState());
        const auto& endState = scenar->state(constraint->endState());

        iscore::MessageList startMessages = flatten(startState.messages().rootNode());
        iscore::MessageList endMessages = flatten(endState.messages().rootNode());

        std::vector<std::pair<const iscore::Message*, const iscore::Message*>> matchingMessages;

        for(auto& message : startMessages)
        {
            if(!message.value.val.isNumeric())
                continue;

            auto it = std::find_if(std::begin(endMessages),
                                   std::end(endMessages),
                                   [&] (const iscore::Message& arg) {
                return message.address == arg.address
                        && arg.value.val.isNumeric()
                        // TODO see CreateSequence (and refactor this) && message.value.val.impl().which() == arg.value.val.impl().which()
                        && message.value != arg.value; });

            if(it != std::end(endMessages))
            {
                // TODO any_of
                auto has_existing_curve = std::find_if(
                            constraint->processes.begin(),
                            constraint->processes.end(),
                            [&] (const Process& proc) {
                    auto ptr = dynamic_cast<const AutomationModel*>(&proc);
                    if(ptr && ptr->address() == message.address)
                        return true;
                    return false;
                });

                if(has_existing_curve != constraint->processes.end())
                    continue;

                matchingMessages.emplace_back(&message, &*it);
            }
        }

        if(!matchingMessages.empty())
        {
            Path<ConstraintModel> constraintPath{*constraint};
            auto macro = new InterpolateMacro{*constraint}; // The constraint already exists

            // Generate brand new ids for the processes
            auto process_ids = getStrongIdRange<Process>(matchingMessages.size(), constraint->processes);

            std::vector<std::pair<Path<SlotModel>, std::vector<Id<LayerModel>>>> slotVec;
            slotVec.reserve(macro->slotsToUse.size());
            // For each slot we have to generate matchingMessages.size() ids.
            for(const auto& elt : macro->slotsToUse)
            {
                if(auto slot = elt.first.try_find())
                {
                    slotVec.push_back({elt.first, getStrongIdRange<LayerModel>(matchingMessages.size(), slot->layers)});
                }
                else
                {
                    slotVec.push_back({elt.first, getStrongIdRange<LayerModel>(matchingMessages.size())});
                }
            }

            int i = 0;
            for(const auto& elt : matchingMessages)
            {
                std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>> layerVec;
                layerVec.reserve(macro->slotsToUse.size());
                std::transform(slotVec.begin(), slotVec.end(), std::back_inserter(layerVec),
                               [&] (const auto& slotVecElt) {
                   return std::make_pair(slotVecElt.first, slotVecElt.second[i]);
                });


                double start = iscore::convert::value<double>(elt.first->value);
                double end = iscore::convert::value<double>(elt.second->value);

                double min = std::min(start, end);
                double max = std::max(start, end);
                if(auto node = iscore::try_getNodeFromAddress(rootNode, elt.first->address))
                {
                    const iscore::AddressSettings& as = node->get<iscore::AddressSettings>();
                    if(as.domain.min.val.isNumeric())
                        min = std::min(min, iscore::convert::value<double>(as.domain.min));

                    if(as.domain.max.val.isNumeric())
                        max = std::max(max, iscore::convert::value<double>(as.domain.max));
                }

                macro->addCommand(new CreateCurveFromStates{
                                      Path<ConstraintModel>{constraintPath},
                                      layerVec,
                                      process_ids[i],
                                      elt.first->address,
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
