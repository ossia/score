#pragma once

#include <iscore/tools/std/Optional.hpp>

#include "MoveEventOnCreationMeta.hpp"
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore_plugin_scenario_export.h>

struct DataStreamInput;
struct DataStreamOutput;

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, both vertical and horizontal move are allowed
 */

namespace Scenario
{
class EventModel;
class ConstraintModel;
class ScenarioModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT MoveNewEvent final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveNewEvent, "Move a new event")
        public:
            MoveNewEvent(
                Path<Scenario::ScenarioModel>&& scenarioPath,
                Id<ConstraintModel> constraintId,
                Id<EventModel> eventId,
                TimeValue date,
                const double y,
                bool yLocked);
        MoveNewEvent(
                Path<Scenario::ScenarioModel>&& scenarioPath,
                Id<ConstraintModel> constraintId,
                Id<EventModel> eventId,
                TimeValue date,
                const double y,
                bool yLocked,
                ExpandMode);

        void undo() const override;
        void redo() const override;

        void update(
                unused_t,
                unused_t,
                const Id<EventModel>& id,
                const TimeValue& date,
                const double y,
                bool yLocked)
        {
            m_cmd.update(id, date, y, ExpandMode::Scale);
            m_y = y;
            m_yLocked = yLocked;
        }

        const Path<Scenario::ScenarioModel>& path() const
        { return m_path; }

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<Scenario::ScenarioModel> m_path;
        Id<ConstraintModel> m_constraintId{};

        MoveEventOnCreationMeta m_cmd;
        double m_y{};
        bool m_yLocked{true}; // default is true and constraints are on the same y.
};
}
}
