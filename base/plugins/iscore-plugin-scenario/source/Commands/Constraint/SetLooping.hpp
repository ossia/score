#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ConstraintModel;
class SetLooping : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), "SetLooping", "SetLooping")

    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SetLooping)

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

