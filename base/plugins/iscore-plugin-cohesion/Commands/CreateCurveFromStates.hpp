#pragma once
#include <Commands/IScoreCohesionCommandFactory.hpp>
#include "base/plugins/iscore-plugin-scenario/source/Commands/Constraint/AddProcessToConstraint.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Address.hpp>

class ConstraintModel;

class CreateCurveFromStates : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(IScoreCohesionCommandFactoryName(), CreateCurveFromStates, "CreateCurveFromStates")
    public:
        CreateCurveFromStates(
                Path<ConstraintModel>&& constraint,
                const iscore::Address &address,
                double start,
                double end);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Scenario::Command::AddProcessToConstraint m_addProcessCmd;

        iscore::Address m_address;

        double m_start{}, m_end{};

};
