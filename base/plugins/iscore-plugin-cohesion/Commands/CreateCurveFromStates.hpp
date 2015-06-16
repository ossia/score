#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/Address.hpp>
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
        ISCORE_COMMAND_DECL("CreateCurveFromStates", "CreateCurveFromStates")
    public:
        CreateCurveFromStates();
        ~CreateCurveFromStates();
        CreateCurveFromStates(ObjectPath&& constraint,
                    const Address &address,
                    double start,
                    double end);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Scenario::Command::AddProcessToConstraint* m_addProcessCmd{};

        Address m_address;

        double m_start{}, m_end{};

};
