#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ExpandMode.hpp>

class EventModel;
class TimeNodeModel;
class ConstraintModel;
class ConstraintViewModel;
class RackModel;
class ScenarioModel;

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
                ISCORE_COMMAND_DECL("MoveEvent", "MoveEvent")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(MoveEvent, "ScenarioControl")
                MoveEvent(
                    ModelPath<ScenarioModel>&& scenarioPath,
                    const id_type<EventModel>& eventId,
                    const TimeValue& date,
                    ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;

                void update(
                        const ModelPath<ScenarioModel>&,
                        const id_type<EventModel>& ,
                        const TimeValue& date,
                        ExpandMode)
                {
                    m_newDate = date;
                }

                const ModelPath<ScenarioModel>& path() const
                { return m_path; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ScenarioModel> m_path;
                id_type<EventModel> m_eventId {};

                TimeValue m_oldDate {};
                TimeValue m_newDate {};

                ExpandMode m_mode{ExpandMode::Scale};

                // Data to correctly restore the processes on undo
                QVector<id_type<TimeNodeModel>> m_movableTimenodes;

                QVector<
                    QPair<
                        QPair<
                            ModelPath<ConstraintModel>,
                            QByteArray
                        >, // The constraint data
                        QMap< // Mapping for the view models of this constraint
                            id_type<ConstraintViewModel>,
                            id_type<RackModel>
                        >
                     >
                > m_savedConstraints;
        };

    }
}
