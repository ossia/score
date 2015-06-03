#include "AutomationControl.hpp"

#include "Commands/AddPoint.hpp"
#include "Commands/RemovePoint.hpp"
#include "Commands/MovePoint.hpp"
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
    if(name == "AddPoint")
    {
        cmd = new AddPoint;
    }
    else if(name == "RemovePoint")
    {
        cmd = new RemovePoint;
    }
    else if(name == "MovePoint")
    {
        cmd = new MovePoint;
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
