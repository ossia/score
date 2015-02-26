#pragma once
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include "Deck/MoveProcessViewModel.hpp"
#include <ProcessInterface/ProcessViewModelInterface.hpp>
#include "RemoveDeckFromBox.hpp"
#include "core/interface/document/DocumentInterface.hpp"
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The MergeDecks class
		 *
		 * Merges a Deck into another.
		 * This moves all the PVMs of the first deck, the source, into the second, the target,
		 * and deletes the source.
		 */
		class MergeDecks : public iscore::AggregateCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				MergeDecks(const ObjectPath& mergeSource,
						   const ObjectPath& mergeTarget):
					AggregateCommand{
						"ScenarioControl",
						"MergeDeck",
						QObject::tr("Merge decks")}
				{
					auto sourcedeck = mergeSource.find<DeckModel>();

					for(ProcessViewModelInterface* pvm : sourcedeck->processViewModels())
					{
						addCommand(new MoveProcessViewModel(iscore::IDocument::path(pvm),
															ObjectPath{mergeTarget}));
					}

					addCommand(new RemoveDeckFromBox{ObjectPath{mergeSource}});
				}
		};
	}
}
