#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
class StateModel;
namespace Scenario
{
    namespace Command
    {
        class ClearState final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ClearState, "ClearState")
            public:
                ClearState(Path<StateModel>&& path);
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<StateModel> m_path;

                QByteArray m_serializedStates;
        };
    }
}
