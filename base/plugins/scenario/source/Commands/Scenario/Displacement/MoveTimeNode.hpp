#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include "MoveEvent.hpp"

class EventModel;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        class MoveTimeNode : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                MoveTimeNode();
                MoveTimeNode(ObjectPath&& scenarioPath,
                             id_type<EventModel> eventId,
                             const TimeValue& date,
                             double height);
                ~MoveTimeNode();
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                MoveEvent* m_cmd{};
        };
    }
}
