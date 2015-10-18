#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
class StateModel;
namespace Scenario
{
    namespace Command
    {
        class ClearState : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), "ClearState", "ClearState")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ClearState)
                ClearState(Path<StateModel>&& path);
                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<StateModel> m_path;

                QByteArray m_serializedStates;
        };
    }
}
