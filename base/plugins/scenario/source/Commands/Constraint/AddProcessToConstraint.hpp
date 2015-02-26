#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessSharedModelInterface;
namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The AddProcessToConstraint class
        */
        class AddProcessToConstraint : public iscore::SerializableCommand
        {
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                AddProcessToConstraint();
                AddProcessToConstraint (ObjectPath&& constraintPath, QString process);

                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith (const QUndoCommand* other) override;

                id_type<ProcessSharedModelInterface> processId() const
                {
                    return m_createdProcessId;
                }

            protected:
                virtual void serializeImpl (QDataStream&) const override;
                virtual void deserializeImpl (QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_processName;

                id_type<ProcessSharedModelInterface> m_createdProcessId {};
        };
    }
}
