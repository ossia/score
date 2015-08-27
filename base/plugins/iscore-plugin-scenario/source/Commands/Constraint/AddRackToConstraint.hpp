#pragma once
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
        class AddRackToConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("AddRackToConstraint", "AddRackToConstraint")
#include <tests/helpers/FriendDeclaration.hpp>

            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddRackToConstraint, "ScenarioControl")
                AddRackToConstraint(Path<ConstraintModel>&& constraintPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<ConstraintModel> m_path;

                Id<RackModel> m_createdRackId {};
        };
    }
}
