#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/State.hpp>

namespace Scenario
{
    namespace Command
    {
        class RemoveStateFromStateModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("RemoveStateFromStateModel", "RemoveStateFromStateModel")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveStateFromStateModel, "ScenarioControl")
                RemoveStateFromStateModel(ObjectPath&& eventPath, const iscore::State& state);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                iscore::State m_state;
        };

    }
}
