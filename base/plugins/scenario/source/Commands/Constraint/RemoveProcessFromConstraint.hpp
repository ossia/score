#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <public_interface/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessSharedModelInterface;
namespace Scenario
{
    namespace Command
    {
        class RemoveProcessFromConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveProcessFromConstraint, "ScenarioControl")
                RemoveProcessFromConstraint(ObjectPath&& constraintPath, id_type<ProcessSharedModelInterface> processId);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ProcessSharedModelInterface> m_processId;

                QByteArray m_serializedProcessData; // Should be done in the constructor
        };
    }
}
