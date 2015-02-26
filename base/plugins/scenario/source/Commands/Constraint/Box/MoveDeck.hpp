#pragma once
#include "CopyDeck.hpp"
#include "RemoveDeckFromBox.hpp"
#include <core/presenter/command/AggregateCommand.hpp>

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
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveDeck(const ObjectPath& deckToMove,
                         ObjectPath&& targetBox) :
                    AggregateCommand
                {
                    "ScenarioControl", "MoveDeck", QObject::tr("Move a deck"),
                    new CopyDeck{ObjectPath{deckToMove}, std::move(targetBox) },
                    new RemoveDeckFromBox{ObjectPath{deckToMove}}
                }
                {

                }
        };
    }
}
