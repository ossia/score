#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Address.hpp>

class ConstraintModel;
namespace Scenario
{
namespace Command
{
class AddProcessToConstraint;
}
}

class UpdateCurve;

class CreateCurveFromStates : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("IScoreCohesionControl", "CreateCurveFromStates", "CreateCurveFromStates")
    public:
        CreateCurveFromStates();
        ~CreateCurveFromStates();
        CreateCurveFromStates(
                ModelPath<ConstraintModel>&& constraint,
                const iscore::Address &address,
                double start,
                double end);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Scenario::Command::AddProcessToConstraint* m_addProcessCmd{};

        iscore::Address m_address;

        double m_start{}, m_end{};

};
