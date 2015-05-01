#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

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
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AddProcessToConstraint, "ScenarioControl")
                AddProcessToConstraint(ObjectPath&& constraintPath, QString process);

                virtual void undo() override;
                virtual void redo() override;

                const ObjectPath& constraintPath() const
                { return m_path; }
                id_type<ProcessSharedModelInterface> processId() const
                {
                    return m_createdProcessId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_processName;

                id_type<ProcessSharedModelInterface> m_createdProcessId {};
        };
    }
}
