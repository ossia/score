#pragma once
#include <Document/Constraint/Rack/RackModel.hpp>
#include <Document/Constraint/Rack/Slot/SlotModel.hpp>
#include "Rack/MoveSlot.hpp"
#include "RemoveRackFromConstraint.hpp"
#include "iscore/document/DocumentInterface.hpp"
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The MergeRackes class
         *
         * Merges a Rack into another.
         */
        class MergeRackes : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("ScenarioControl", "MergeRackes", "MergeRackes")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MergeRackes():
                      AggregateCommand{factoryName(),
                                       commandName(),
                                       description()} { }

                MergeRackes(const Path<RackModel>& mergeSource,
                           const Path<RackModel>& mergeTarget) :
                    AggregateCommand{factoryName(),
                                     commandName(),
                                     description()}
                {
                    const auto& sourcerack = mergeSource.find();

                    for(const auto& slot : sourcerack.slotmodels)
                    {
                        addCommand(new MoveSlot{
                                       slot,
                                       Path<RackModel>{mergeTarget}});
                    }

                    addCommand(new RemoveRackFromConstraint{Path<RackModel>{mergeSource}});
                }
        };
    }
}
