#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/State.hpp>

class StateModel;
namespace Scenario
{
    namespace Command
    {
        // TODO rename file
        class AddStateToStateModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL2("ScenarioControl", "AddStateToStateModel", "AddStateToStateModel")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AddStateToStateModel)
                AddStateToStateModel(
                    Path<StateModel>&& path,
                  const iscore::StatePath& parent_path,
                        const iscore::StateNode& state,
                        int posInParent);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<StateModel> m_path;

                iscore::StatePath m_parentPath;
                iscore::StateNode m_state;
                int m_pos{};
        };
    }
}
