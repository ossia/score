#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/SettableIdentifier.hpp>
#include <tools/ObjectPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include "MoveEvent.hpp"

struct EventData;
class EventModel;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        class MoveTimeNode : public iscore::SerializableCommand
        {
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveTimeNode();
                MoveTimeNode (ObjectPath&& scenarioPath, EventData data);
                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith (const QUndoCommand* other) override;
                TimeValue m_oldX {}; // TODO : bof bof !

            protected:
                virtual void serializeImpl (QDataStream&) const override;
                virtual void deserializeImpl (QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<EventModel> m_eventId {};

                double m_oldHeightPosition {};
                double m_newHeightPosition {};
                TimeValue m_newX {};
        };
    }
}
