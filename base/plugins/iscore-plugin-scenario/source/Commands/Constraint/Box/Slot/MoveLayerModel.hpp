/*#pragma once
#include "CopyLayerModel.hpp"
#include "RemoveLayerModelFromSlot.hpp"
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        //
        // @brief The MoveLayerModel class
        //
        // Moves a process view from a Slot to another.
        // Note : this must be in the same constraint.
        //s
        class MoveLayerModel : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveLayerModel():
                    AggregateCommand{"ScenarioControl",
                                     commandName(),
                                     description()} { }

                MoveLayerModel(const ObjectPath& pvmToMove,
                                     ObjectPath&& targetSlot) :
                    AggregateCommand{"ScenarioControl",
                                     "MoveLayerModel",
                                     QObject::tr("Move a process view model"),
                                     new CopyLayerModel{ObjectPath{pvmToMove}, std::move(targetSlot) }}
        {
            auto cmd = new RemoveLayerModelFromSlot{
                    ObjectPath{pvmToMove},
                    id_type<LayerModel>(
                        ObjectIdentifierVector(pvmToMove.vec()).takeLast().id())};
            addCommand(cmd);


        }
        };
    }
}*/
