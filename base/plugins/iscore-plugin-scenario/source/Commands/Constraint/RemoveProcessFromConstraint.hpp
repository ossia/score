#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessModel;
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
                RemoveProcessFromConstraint(ObjectPath&& constraintPath, id_type<ProcessModel> processId);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ProcessModel> m_processId;

                QByteArray m_serializedProcessData;
                QVector<QPair<ObjectPath, QByteArray>> m_serializedViewModels;
        };
    }
}
