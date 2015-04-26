#pragma once

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class TimeNodeModel;
class EventModel;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        class CreateEventOnTimeNode : public iscore::SerializableCommand
        {
            ISCORE_COMMAND
            #include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CreateEventOnTimeNode, "ScenarioControl")
                CreateEventOnTimeNode(
                    ObjectPath&& scenarioPath,
                    id_type<TimeNodeModel> timeNodeId,
                    double heightPosition);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

                auto createdEventId() const
                { return m_createdEventId; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<TimeNodeModel> m_timeNodeId;
                double m_heightPosition{};

                id_type<EventModel> m_createdEventId;
        };
    }
}
