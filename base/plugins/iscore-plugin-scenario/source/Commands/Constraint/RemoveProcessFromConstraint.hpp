#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class Process;
class ConstraintModel;
class LayerModel;
namespace Scenario
{
    namespace Command
    {
        class RemoveProcessFromConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("RemoveProcessFromConstraint", "RemoveProcessFromConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveProcessFromConstraint, "ScenarioControl")
                RemoveProcessFromConstraint(
                    ModelPath<ConstraintModel>&& constraintPath,
                    const id_type<Process>& processId);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ConstraintModel> m_path;
                id_type<Process> m_processId;

                QByteArray m_serializedProcessData;
                QVector<QPair<ModelPath<LayerModel>, QByteArray>> m_serializedViewModels;
        };
    }
}
