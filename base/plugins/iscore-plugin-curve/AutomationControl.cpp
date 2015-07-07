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

iscore::SerializableCommand* AutomationControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<AutomationCommandFactory>(name, data);
}
