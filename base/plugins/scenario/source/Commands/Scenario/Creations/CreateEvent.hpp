#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include <iscore/tools/ObjectPath.hpp>

#include <QPointF>

struct EventModelData;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The CreateEventCommand class
        *
        * This command creates an Event, which is linked to the first event in the
        * scenario.
        */
        class CreateEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                CreateEvent();
                ~CreateEvent();
                CreateEvent(ObjectPath&& scenarioPath, EventData data);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

                id_type<TimeNodeModel> createdTimeNode() const
                {
                    return m_cmd->createdTimeNode();
                }

                id_type<EventModel> createdEvent() const
                {
                    if (m_tn)
                        return m_tnCmd->createdEvent();
                    return m_cmd->createdEvent();
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                CreateEventAfterEvent* m_cmd {};
                CreateEventAfterEventOnTimeNode* m_tnCmd {};

                bool m_tn;
        };
    }
}
