#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class RackModel;

namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddRackToConstraint class
         *
         * Adds an empty rack, with no slots, to a constraint.
         */
        class AddRackToConstraint final : public iscore::SerializableCommand
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
