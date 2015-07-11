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
                ISCORE_COMMAND_DECL("MergeRackes", "MergeRackes")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MergeRackes():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

                MergeRackes(const ObjectPath& mergeSource,
                           const ObjectPath& mergeTarget) :
                    AggregateCommand{"ScenarioControl",
                                     commandName(),
                                     description()}
                {
                    auto& sourcerack = mergeSource.find<RackModel>();

                    for(const auto& slot : sourcerack.getSlots())
                    {
                        addCommand(new MoveSlot{
                                       iscore::IDocument::path(slot),
                                       ObjectPath {mergeTarget}});
                    }

                    addCommand(new RemoveRackFromConstraint{ObjectPath{mergeSource}});
                }
        };
    }
}
