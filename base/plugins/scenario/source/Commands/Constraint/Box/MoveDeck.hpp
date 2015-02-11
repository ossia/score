#pragma once
#include "CopyDeck.hpp"
#include "RemoveDeckFromBox.hpp"
#include "Commands/Aggregate/AggregateCommand.hpp"

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
		class MoveDeck : public AggregateCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				MoveDeck(const ObjectPath& deckToMove,
						 ObjectPath&& targetBox):
					AggregateCommand{
						"ScenarioControl", "MoveDeck", QObject::tr("Move a deck"),
						new CopyDeck{ObjectPath{deckToMove}, std::move(targetBox)},
						new RemoveDeckFromBox{ObjectPath{deckToMove}}}
				{

				}
		};
	}
}

#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include "Deck/MoveProcessViewModel.hpp"
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The MergeDeck class
		 *
		 * Merges a Deck into another.
		 * This moves all the PVMs of the first deck into the second, and deletes the
		 * first deck
		 */
		class MergeDeck : public AggregateCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				MergeDeck(const ObjectPath& mergeSource,
						  ObjectPath&& mergeTarget):
					AggregateCommand{
						"ScenarioControl", "MergeDeck", QObject::tr("Merge decks"),
		//				new CopyDeck{ObjectPath{mergeSource}, std::move(targetBox)},
		//				new RemoveDeckFromBox{ObjectPath{deckToMove}}
		}
				{
					auto sourcedeck = mergeSource.find<DeckModel>();

				//	for(ProcessV)
				}
		};
	}
}
