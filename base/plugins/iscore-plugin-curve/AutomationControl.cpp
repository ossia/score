#include "AutomationControl.hpp"

#include "Commands/AddPoint.hpp"
#include "Commands/RemovePoint.hpp"
#include "Commands/MovePoint.hpp"
#include "Commands/ChangeAddress.hpp"

AutomationControl::AutomationControl(QObject* parent) :
    PluginControlInterface {"AutomationControl", parent}
{

}


iscore::SerializableCommand* AutomationControl::instantiateUndoCommand(const QString& name, const QByteArray& data)
{
    iscore::SerializableCommand* cmd {};

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

    else
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;

}
