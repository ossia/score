/*
#pragma once
#include <Document/Constraint/Rack/Slot/SlotModel.hpp>
#include "Slot/MoveLayerModel.hpp"
#include <ProcessInterface/LayerModel.hpp>
#include "RemoveSlotFromRack.hpp"
#include "iscore/document/DocumentInterface.hpp"
namespace Scenario
{
    namespace Command
    {
        ///
        // @brief The MergeSlots class
        //
        // Merges a Slot into another.
        // This moves all the LMs of the first slot, the source, into the second, the target,
        // and deletes the source.
        //
        class MergeSlots : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MergeSlots():
                  AggregateCommand{"ScenarioControl",
                                   commandName(),
                                   description()} { }

                MergeSlots(const ObjectPath& mergeSource,
                           const ObjectPath& mergeTarget) :
                    AggregateCommand{"ScenarioControl",
                                     commandName(),
                                     description()}
                {
                    auto sourceslot = mergeSource.find<SlotModel>();

                    for(LayerModel* lm : sourceslot->layerModels())
                    {
                        addCommand(new MoveLayerModel(lm,
                        ObjectPath {mergeTarget}));
                    }

                    addCommand(new RemoveSlotFromRack{ObjectPath{mergeSource}});
                }
        };
    }
}
*/
