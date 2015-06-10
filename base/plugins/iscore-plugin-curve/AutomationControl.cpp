#include "AutomationControl.hpp"

#include "Curve/Commands/UpdateCurve.hpp"
#include "Commands/ChangeAddress.hpp"
#include "Commands/SetCurveMin.hpp"
#include "Commands/SetCurveMax.hpp"
AutomationControl::AutomationControl(
        iscore::Presenter* pres) :
    PluginControlInterface {pres, "AutomationControl", nullptr}
{
}


iscore::SerializableCommand* AutomationControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    iscore::SerializableCommand* cmd {};

    // TODO harmonize this
    // TODO className -> commandName
    if(name == UpdateCurve::className())
    {
        cmd = new UpdateCurve;
    }
    else if(name == "ChangeAddress")
    {
        cmd = new ChangeAddress;
    }
    else if(name == SetCurveMin::className())
    {
        cmd = new SetCurveMin;
    }
    else if(name == SetCurveMax::className())
    {
        cmd = new SetCurveMax;
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;
}
