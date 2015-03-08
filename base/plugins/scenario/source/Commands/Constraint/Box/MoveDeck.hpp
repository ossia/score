#pragma once
#include "CopyDeck.hpp"
#include "RemoveDeckFromBox.hpp"
#include <public_interface/command/AggregateCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The MoveDeck class
         *
         * Moves a Deck from a Box to another (should not be the same :) )
         * Note : this must be in the same constraint.
         */
        class MoveDeck : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveDeck():
                      AggregateCommand{"ScenarioControl",
                                       className(),
                                       description()} { }

                MoveDeck(const ObjectPath& deckToMove,
                         ObjectPath&& targetBox) :
                    AggregateCommand {"ScenarioControl",
                                      className(),
                                      description(),
                                      new CopyDeck{ObjectPath{deckToMove}, std::move(targetBox) },
                                      new RemoveDeckFromBox{ObjectPath{deckToMove}}}
                {

                }
        };
    }
}
