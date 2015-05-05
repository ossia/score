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
class BoxModel;

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
                  ExpandMode mode,
                  bool changeDate = true);

                virtual void undo() override;
                virtual void redo() override;

                void update(const ObjectPath&,
                            const id_type<EventModel>& ,
                            const TimeValue& date,
                            double height,
                            ExpandMode)
                {
                    m_newDate = date;
                    m_newHeightPosition = height;
                }

                TimeValue m_oldDate {}; // TODO : bof bof !


                const ObjectPath& path() const
                { return m_path; }
                double heightPosition() const
                { return m_newHeightPosition; }

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
                            id_type<BoxModel>
                        >
                     >
               > m_savedConstraints;
                bool m_changeDate{};
        };

    }
}
