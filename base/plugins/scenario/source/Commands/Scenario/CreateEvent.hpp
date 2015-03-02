#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include <tools/ObjectPath.hpp>

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
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                CreateEventAfterEvent* m_cmd {};
        };
    }
}
