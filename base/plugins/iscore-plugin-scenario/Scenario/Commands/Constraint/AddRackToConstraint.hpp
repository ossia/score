#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class RackModel;
class ConstraintModel;

namespace Command
{
/**
         * @brief The AddRackToConstraint class
         *
         * Adds an empty rack, with no slots, to a constraint.
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT AddRackToConstraint final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddRackToConstraint, "Add a rack")

        public:
            AddRackToConstraint(Path<ConstraintModel>&& constraintPath);

        void undo() const override;
        void redo() const override;

        const auto& createdRack() const
        { return m_createdRackId; }

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<ConstraintModel> m_path;

        Id<RackModel> m_createdRackId {};
};
}
}
