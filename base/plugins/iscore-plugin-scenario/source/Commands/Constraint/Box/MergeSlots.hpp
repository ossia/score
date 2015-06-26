/*
#pragma once
#include <Document/Constraint/Box/Slot/SlotModel.hpp>
#include "Slot/MoveLayerModel.hpp"
#include <ProcessInterface/LayerModel.hpp>
#include "RemoveSlotFromBox.hpp"
#include "iscore/document/DocumentInterface.hpp"
namespace Scenario
{
    namespace Command
    {
        ///
        // @brief The MergeSlots class
        //
        // Merges a Slot into another.
        // This moves all the PVMs of the first slot, the source, into the second, the target,
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

                    for(LayerModel* pvm : sourceslot->layerModels())
                    {
                        addCommand(new MoveLayerModel(iscore::IDocument::path(pvm),
                        ObjectPath {mergeTarget}));
                    }

                    addCommand(new RemoveSlotFromBox{ObjectPath{mergeSource}});
                }
        };
    }
}
*/
