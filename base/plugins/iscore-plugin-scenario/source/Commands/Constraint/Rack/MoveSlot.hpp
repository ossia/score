#pragma once
#include "CopySlot.hpp"
#include "RemoveSlotFromRack.hpp"
#include <iscore/command/AggregateCommand.hpp>

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
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveSlot():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

                MoveSlot(const ObjectPath& slotToMove,
                         ObjectPath&& targetRack) :
                    AggregateCommand {"ScenarioControl",
                                      commandName(),
                                      description(),
                                      new CopySlot{ObjectPath{slotToMove}, std::move(targetRack) },
                                      new RemoveSlotFromRack{ObjectPath{slotToMove}}}
                {

                }
        };
    }
}
