#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

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

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ProcessSharedModelInterface> m_processId;

                QByteArray m_serializedProcessData;
                QMap<std::tuple<int, int, int>, QByteArray>  m_serializedViewModels;
        };
    }
}
