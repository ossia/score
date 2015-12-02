#pragma once
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include "Rack/MoveSlot.hpp"
#include "RemoveRackFromConstraint.hpp"
#include <iscore/document/DocumentInterface.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The MergeRackes class
         *
         * Merges a Rack into another.
         */
        class MergeRackes final : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MergeRackes, "Merge racks")
            public:
                MergeRackes(const Path<RackModel>& mergeSource,
                           const Path<RackModel>& mergeTarget)
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
