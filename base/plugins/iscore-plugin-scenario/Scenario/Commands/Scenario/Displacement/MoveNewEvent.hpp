#pragma once

#include <boost/optional/optional.hpp>

#include "MoveEventOnCreationMeta.hpp"
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore_plugin_scenario_export.h>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class EventModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, both vertical and horizontal move are allowed
 */

namespace Scenario
{
    namespace Command
    {
        class ISCORE_PLUGIN_SCENARIO_EXPORT MoveNewEvent final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveNewEvent, "Move a new event")
                public:
                MoveNewEvent(
                  Path<Scenario::ScenarioModel>&& scenarioPath,
                    const Id<ConstraintModel>& constraintId,
                    const Id<EventModel>& eventId,
                    const TimeValue& date,
                    const double y,
                    bool yLocked);
                MoveNewEvent(
                        Path<Scenario::ScenarioModel>&& scenarioPath,
                        const Id<ConstraintModel>& constraintId,
                        const Id<EventModel>& eventId,
                        const TimeValue& date,
                        const double y,
                        bool yLocked,
                        ExpandMode);

                void undo() const override;
                void redo() const override;

                void update(
                        const Path<Scenario::ScenarioModel>& path,
                        const Id<ConstraintModel>&,
                        const Id<EventModel>& id,
                        const TimeValue& date,
                        const double y,
                        bool yLocked)
                {
                    m_cmd.update(path, id, date, ExpandMode::Scale);
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
