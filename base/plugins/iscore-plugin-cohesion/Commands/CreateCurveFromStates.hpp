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
        ISCORE_COMMAND_DECL("IScoreCohesionControl", "CreateCurveFromStates", "CreateCurveFromStates")
    public:
        CreateCurveFromStates();
        ~CreateCurveFromStates();
        CreateCurveFromStates(
                Path<ConstraintModel>&& constraint,
                const iscore::Address &address,
                double start,
                double end);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Scenario::Command::AddProcessToConstraint* m_addProcessCmd{};

        iscore::Address m_address;

        double m_start{}, m_end{};

};
