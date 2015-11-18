#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <Process/TimeValue.hpp>

class ConstraintModel;
class SetLooping final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetLooping, "SetLooping")

    public:

        SetLooping(
                Path<ConstraintModel>&& constraintPath,
                bool looping);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream& s) const override;
        void deserializeImpl(QDataStream& s) override;

    private:
        Path<ConstraintModel> m_path;
        bool m_looping{};
};

