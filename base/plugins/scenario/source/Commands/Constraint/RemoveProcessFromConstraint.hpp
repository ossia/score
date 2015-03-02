#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

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
                RemoveProcessFromConstraint();
                RemoveProcessFromConstraint(ObjectPath&& constraintPath, id_type<ProcessSharedModelInterface> processId);
                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

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
