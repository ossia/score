/*
#pragma once
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include "Deck/MoveProcessViewModel.hpp"
#include <ProcessInterface/ProcessViewModelInterface.hpp>
#include "RemoveDeckFromBox.hpp"
#include "iscore/document/DocumentInterface.hpp"
namespace Scenario
{
    namespace Command
    {
        ///
        // @brief The MergeDecks class
        //
        // Merges a Deck into another.
        // This moves all the PVMs of the first deck, the source, into the second, the target,
        // and deletes the source.
        //
        class MergeDecks : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MergeDecks():
                  AggregateCommand{"ScenarioControl",
                                   className(),
                                   description()} { }

                MergeDecks(const ObjectPath& mergeSource,
                           const ObjectPath& mergeTarget) :
                    AggregateCommand{"ScenarioControl",
                                     className(),
                                     description()}
                {
                    auto sourcedeck = mergeSource.find<DeckModel>();

                    for(ProcessViewModelInterface* pvm : sourcedeck->processViewModels())
                    {
                        addCommand(new MoveProcessViewModel(iscore::IDocument::path(pvm),
                        ObjectPath {mergeTarget}));
                    }

                    addCommand(new RemoveDeckFromBox{ObjectPath{mergeSource}});
                }
        };
    }
}
*/
