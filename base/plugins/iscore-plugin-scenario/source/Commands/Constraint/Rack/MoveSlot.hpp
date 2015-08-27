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

                MoveSlot(const Path<SlotModel>& slotToMove,
                         Path<RackModel>&& targetRack) :
                    AggregateCommand {"ScenarioControl",
                                      commandName(),
                                      description(),
                                      new CopySlot{Path<SlotModel>{slotToMove}, std::move(targetRack) },
                                      new RemoveSlotFromRack{Path<SlotModel>{slotToMove}}}
                {

                }
        };
    }
}
