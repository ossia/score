#pragma once
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
class StateModel;
namespace Scenario
{
    namespace Command
    {
        class ClearState : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL2("ScenarioControl", "ClearState", "ClearState")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(ClearState)
                ClearState(ModelPath<StateModel>&& path);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<StateModel> m_path;

                QByteArray m_serializedStates;
        };
    }
}
