#pragma once
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include "Box/MoveDeck.hpp"
#include "RemoveBoxFromConstraint.hpp"
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The MergeBoxes class
		 *
		 * Merges a Box into another.
		 */
		class MergeBoxes : public AggregateCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				MergeBoxes(const ObjectPath& mergeSource,
						   const ObjectPath& mergeTarget):
					AggregateCommand{
						"ScenarioControl",
						"MergeBoxes",
						QObject::tr("Merge decks")}
				{
					auto sourcebox = mergeSource.find<BoxModel>();

					for(DeckModel* deck : sourcebox->decks())
					{
						addCommand(new MoveDeck(ObjectPath::pathFromObject(deck),
												ObjectPath{mergeTarget}));
					}

					addCommand(new RemoveBoxFromConstraint{ObjectPath{mergeSource}});
				}
		};
	}
}
