#pragma once
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
        class RemoveRackFromConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("RemoveRackFromConstraint", "RemoveRackFromConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveRackFromConstraint, "ScenarioControl")
                RemoveRackFromConstraint(
                        ModelPath<RackModel>&& rackPath);
                RemoveRackFromConstraint(
                        ModelPath<ConstraintModel>&& constraintPath,
                        id_type<RackModel> rackId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ConstraintModel> m_path;
                id_type<RackModel> m_rackId {};

                QByteArray m_serializedRackData; // Should be done in the constructor

                QMap<id_type<ConstraintViewModel>, bool> m_rackMappings;
        };
    }
}
