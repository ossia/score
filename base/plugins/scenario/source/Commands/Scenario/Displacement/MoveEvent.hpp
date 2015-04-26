#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ExpandMode.hpp>

class EventModel;
class TimeNodeModel;

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
                MoveEvent(ObjectPath&& scenarioPath,
                  id_type<EventModel> eventId,
                  const TimeValue& date,
                  double height,
                  ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;
                TimeValue m_oldDate {}; // TODO : bof bof !

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<EventModel> m_eventId {};

                double m_oldHeightPosition {};
                double m_newHeightPosition {};
                TimeValue m_newDate {};

                ExpandMode m_mode{ExpandMode::Scale};

                QVector<id_type<TimeNodeModel>> m_movableTimenodes;
        };
    }
}
