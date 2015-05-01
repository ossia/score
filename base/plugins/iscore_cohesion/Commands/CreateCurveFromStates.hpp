#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
namespace Scenario
{
namespace Command
{
class AddProcessToConstraint;
}
}

class CreateCurveFromStates : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateCurveFromStates", "CreateCurveFromStates")
    public:
        CreateCurveFromStates();
        ~CreateCurveFromStates();
        CreateCurveFromStates(ObjectPath&& constraint,
                    QString address,
                    double start,
                    double end);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Scenario::Command::AddProcessToConstraint* m_cmd{};
        QString m_address;

        double m_start{}, m_end{};

};
