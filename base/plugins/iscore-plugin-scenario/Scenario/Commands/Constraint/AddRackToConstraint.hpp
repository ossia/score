#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class RackModel;
class ConstraintModel;
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
#include <tests/helpers/FriendDeclaration.hpp>

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
