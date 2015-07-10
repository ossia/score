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
        class MoveBaseEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("MoveBaseEvent", "MoveBaseEvent")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(MoveBaseEvent, "ScenarioControl")
                MoveBaseEvent(ObjectPath&& scenarioPath,
                  const TimeValue& date,
                  ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;

                void update(const ObjectPath&,
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

                TimeValue m_oldDate {};
                TimeValue m_newDate {};

                ExpandMode m_mode{ExpandMode::Scale};

                QPair<
                    QByteArray, // The constraint data
                    QMap< // Mapping for the view models of this constraint
                        id_type<AbstractConstraintViewModel>,
                        id_type<RackModel>
                    >
                > m_savedConstraint;
        };

    }
}
