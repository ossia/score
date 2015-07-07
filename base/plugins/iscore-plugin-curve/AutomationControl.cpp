#include "AutomationControl.hpp"

#include "Curve/Commands/UpdateCurve.hpp"
#include "Curve/Commands/SetSegmentParameters.hpp"

#include "Commands/ChangeAddress.hpp"
#include "Commands/SetCurveMin.hpp"
#include "Commands/SetCurveMax.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
AutomationControl::AutomationControl(
        iscore::Presenter* pres) :
    PluginControlInterface {pres, "AutomationControl", nullptr}
{
    setupCommands();
}

struct AutomationCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap AutomationCommandFactory::map;

void AutomationControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
                UpdateCurve,
                SetSegmentParameters,
                ChangeAddress,
                SetCurveMin,
                SetCurveMax
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<AutomationCommandFactory>());
}

// TODO : now this algorithm can finally be generalizer to all Controls
iscore::SerializableCommand* AutomationControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    auto it = AutomationCommandFactory::map.find(name);
    if(it != AutomationCommandFactory::map.end())
    {
        iscore::SerializableCommand* cmd = (*(*it).second)();
        cmd->deserialize(data);
        return cmd;
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }
}
