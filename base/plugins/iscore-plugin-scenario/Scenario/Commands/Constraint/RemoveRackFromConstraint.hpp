#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class ConstraintModel;
class ConstraintViewModel;
class RackModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveRackFromConstraint class
         *
         * Removes a rack : all the slots and function views will be removed.
         */
        class RemoveRackFromConstraint final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveRackFromConstraint, "Remove a rack")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                RemoveRackFromConstraint(
                        Path<RackModel>&& rackPath);
                RemoveRackFromConstraint(
                        Path<ConstraintModel>&& constraintPath,
                        Id<RackModel> rackId);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<ConstraintModel> m_path;
                Id<RackModel> m_rackId {};

                QByteArray m_serializedRackData; // Should be done in the constructor

                QMap<Id<ConstraintViewModel>, bool> m_rackMappings;
        };
    }
}
