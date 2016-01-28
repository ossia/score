#pragma once
#include "CopySlot.hpp"
#include "RemoveSlotFromRack.hpp"
#include <iscore/command/AggregateCommand.hpp>
namespace Scenario
{
class RackModel;

namespace Command
{
/**
         * @brief The MoveSlot class
         *
         * Moves a Slot from a Rack to another (should not be the same :) )
         * Note : this must be in the same constraint.
         */
class MoveSlot final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveSlot, "Move a slot")
        public:

            MoveSlot(const Path<SlotModel>& slotToMove,
                     Path<RackModel>&& targetRack) :
          AggregateCommand {new CopySlot{Path<SlotModel>{slotToMove}, std::move(targetRack) },
                            new RemoveSlotFromRack{Path<SlotModel>{slotToMove}}}
{

}
};
}
}
