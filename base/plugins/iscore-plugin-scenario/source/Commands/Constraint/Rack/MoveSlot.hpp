#pragma once
#include "CopySlot.hpp"
#include "RemoveSlotFromRack.hpp"
#include <iscore/command/AggregateCommand.hpp>

class RackModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The MoveSlot class
         *
         * Moves a Slot from a Rack to another (should not be the same :) )
         * Note : this must be in the same constraint.
         */
        class MoveSlot : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("MoveSlot", "MoveSlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveSlot():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

                MoveSlot(const ModelPath<SlotModel>& slotToMove,
                         ModelPath<RackModel>&& targetRack) :
                    AggregateCommand {"ScenarioControl",
                                      commandName(),
                                      description(),
                                      new CopySlot{ModelPath<SlotModel>{slotToMove}, std::move(targetRack) },
                                      new RemoveSlotFromRack{ModelPath<SlotModel>{slotToMove}}}
                {

                }
        };
    }
}
