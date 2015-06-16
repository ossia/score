#pragma once
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include "Box/MoveDeck.hpp"
#include "RemoveBoxFromConstraint.hpp"
#include "iscore/document/DocumentInterface.hpp"
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The MergeBoxes class
         *
         * Merges a Box into another.
         */
        class MergeBoxes : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MergeBoxes():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

                MergeBoxes(const ObjectPath& mergeSource,
                           const ObjectPath& mergeTarget) :
                    AggregateCommand{"ScenarioControl",
                                     commandName(),
                                     description()}
                {
                    auto& sourcebox = mergeSource.find<BoxModel>();

                    for(const auto& deck : sourcebox.decks())
                    {
                        addCommand(new MoveDeck{
                                       iscore::IDocument::path(deck),
                                       ObjectPath {mergeTarget}});
                    }

                    addCommand(new RemoveBoxFromConstraint{ObjectPath{mergeSource}});
                }
        };
    }
}
