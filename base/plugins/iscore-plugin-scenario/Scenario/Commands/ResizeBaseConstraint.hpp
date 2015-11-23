#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ExpandMode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

class EventModel;
class TimeNodeModel;
class ConstraintModel;
class ConstraintViewModel;
class RackModel;

#include <tests/helpers/ForwardDeclaration.hpp>

// TODO rename file
/*
 * Command to change a constraint duration by moving event. Vertical move is not allowed.
 */
class BaseScenario;
namespace Scenario
{
namespace Command
{
class MoveBaseEvent final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveBaseEvent, "Move base event")
#include <tests/helpers/FriendDeclaration.hpp>
        public:
          MoveBaseEvent(
          Path<BaseScenario>&& scenarioPath,
              const Id<EventModel>&,
            const TimeValue& date,
                  ExpandMode mode);

        void undo() const override;
        void redo() const override;

        void update(
                const Path<BaseScenario>&,
                const Id<EventModel>&,
                const TimeValue& date,
                ExpandMode)
        {
            m_newDate = date;
        }

        const Path<BaseScenario>& path() const
        { return m_path; }

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<BaseScenario> m_path;

        TimeValue m_oldDate {};
        TimeValue m_newDate {};

        ExpandMode m_mode{ExpandMode::Scale};

        QPair<
        QByteArray, // The constraint data
        QMap< // Mapping for the view models of this constraint
        Id<ConstraintViewModel>,
        Id<RackModel>
        >
        > m_savedConstraint;
};

}
}
