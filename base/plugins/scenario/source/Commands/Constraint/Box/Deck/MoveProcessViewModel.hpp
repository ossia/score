/*#pragma once
#include "CopyProcessViewModel.hpp"
#include "RemoveProcessViewModelFromDeck.hpp"
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        //
        // @brief The MoveProcessViewModel class
        //
        // Moves a process view from a Deck to another.
        // Note : this must be in the same constraint.
        //s
        class MoveProcessViewModel : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveProcessViewModel():
                    AggregateCommand{"ScenarioControl",
                                     className(),
                                     description()} { }

                MoveProcessViewModel(const ObjectPath& pvmToMove,
                                     ObjectPath&& targetDeck) :
                    AggregateCommand{"ScenarioControl",
                                     "MoveProcessViewModel",
                                     QObject::tr("Move a process view model"),
                                     new CopyProcessViewModel{ObjectPath{pvmToMove}, std::move(targetDeck) }}
        {
            auto cmd = new RemoveProcessViewModelFromDeck{
                    ObjectPath{pvmToMove},
                    id_type<ProcessViewModelInterface>(
                        ObjectIdentifierVector(pvmToMove.vec()).takeLast().id())};
            addCommand(cmd);


        }
        };
    }
}*/
