#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ExpandMode.hpp>

class EventModel;
class TimeNodeModel;
class ConstraintModel;
class AbstractConstraintViewModel;
class RackModel;

#include <tests/helpers/ForwardDeclaration.hpp>

/*
 * Command to change a constraint duration by moving event. Vertical move is not allowed.
 */

namespace Scenario
{
    namespace Command
    {
        class MoveEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(MoveEvent, "ScenarioControl")
                MoveEvent(ObjectPath&& scenarioPath,
                  id_type<EventModel> eventId,
                  const TimeValue& date,
                  ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;

                void update(const ObjectPath&,
                            const id_type<EventModel>& ,
                            const TimeValue& date,
                            ExpandMode)
                {
                    m_newDate = date;
                }

                const ObjectPath& path() const
                { return m_path; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<EventModel> m_eventId {};

                TimeValue m_oldDate {};
                TimeValue m_newDate {};

                ExpandMode m_mode{ExpandMode::Scale};

                // Data to correctly restore the processes on undo
                QVector<id_type<TimeNodeModel>> m_movableTimenodes;

                QVector<
                    QPair<
                        QPair<
                            ObjectPath,
                            QByteArray
                        >, // The constraint data
                        QMap< // Mapping for the view models of this constraint
                            id_type<AbstractConstraintViewModel>,
                            id_type<RackModel>
                        >
                     >
               > m_savedConstraints;
                bool m_changeDate{};
        };

    }
}
