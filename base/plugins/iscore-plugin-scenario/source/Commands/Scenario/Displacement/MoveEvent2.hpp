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
struct ElementsProperties;

#include <tests/helpers/ForwardDeclaration.hpp>




namespace Scenario
{
    namespace Command
    {
        class MoveEvent2 : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL_OBSOLETE("MoveEvent2", "MoveEvent2")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(MoveEvent2, "ScenarioControl")
                MoveEvent2(
                    Path<ScenarioModel>&& scenarioPath,
                    const Id<EventModel>& eventId,
                    const TimeValue& date,
                    ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;

                void update(
                        const Path<ScenarioModel>&,
                        const Id<EventModel>& ,
                        const TimeValue& date,
                        ExpandMode)
                {
                    m_newDate = date;
                }

                const Path<ScenarioModel>& path() const
                { return m_path; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<ScenarioModel> m_path;
                Id<EventModel> m_eventId {};

                TimeValue m_oldDate {};
                TimeValue m_newDate {};

                ExpandMode m_mode{ExpandMode::Scale};

                // Data to correctly restore the processes on undo
                QVector<Id<TimeNodeModel>> m_movableTimenodes;

                QVector<
                    QPair<
                        QPair<
                            Path<ConstraintModel>,
                            QByteArray
                        >, // The constraint data
                        QMap< // Mapping for the view models of this constraint
                            Id<ConstraintViewModel>,
                            Id<RackModel>
                        >
                     >
                > m_savedConstraints;
        };

    }
}
