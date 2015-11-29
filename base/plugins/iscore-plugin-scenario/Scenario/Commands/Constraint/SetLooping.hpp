#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "iscore/tools/ModelPath.hpp"

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;

class SetLooping final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetLooping, "Change looping status")

    public:

        SetLooping(
                Path<ConstraintModel>&& constraintPath,
                bool looping);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<ConstraintModel> m_path;
        bool m_looping{};
};

