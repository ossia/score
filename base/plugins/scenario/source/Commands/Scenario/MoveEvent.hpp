#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <tools/SettableIdentifier.hpp>
#include <tools/ObjectPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
struct EventData;
class EventModel;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        class MoveEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(MoveEvent, "ScenarioControl")
                MoveEvent(ObjectPath&& scenarioPath, EventData data);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;
                TimeValue m_oldX {}; // TODO : bof bof !

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<EventModel> m_eventId {};

                double m_oldHeightPosition {};
                double m_newHeightPosition {};
                TimeValue m_newX {};
        };
    }
}
