#include "iscore_plugin_automation.hpp"

#include <Curve/Process/CurveProcessFactory.hpp>

#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationView.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationPresenter.hpp>

#if defined(ISCORE_LIB_INSPECTOR)
#include "Automation/Inspector/AutomationInspectorFactory.hpp"
#include "Automation/Inspector/AutomationStateInspectorFactory.hpp"
#endif

#include "Automation/Commands/ChangeAddress.hpp"
#include "Automation/Commands/SetCurveMin.hpp"
#include "Automation/Commands/SetCurveMax.hpp"
#include "Automation/Commands/InitAutomation.hpp"

DEFINE_CURVE_PROCESS_FACTORY(
        AutomationFactory,
        "Automation",
        AutomationModel,
        AutomationLayerModel,
        AutomationPresenter,
        AutomationView,
        AutomationColors)

iscore_plugin_automation::iscore_plugin_automation() :
    QObject {}
{
    AutomationFactory f;
}

QVector<iscore::FactoryInterface*> iscore_plugin_automation::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new AutomationFactory};
    }

#if defined(ISCORE_LIB_INSPECTOR)
    if(factoryName == InspectorWidgetFactory::factoryName())
    {
        return {new AutomationInspectorFactory,
                new AutomationStateInspectorFactory};
    }
#endif
    return {};
}

std::pair<const std::string, CommandGeneratorMap> iscore_plugin_automation::make_commands()
{
    std::pair<const std::string, CommandGeneratorMap> cmds;
    boost::mpl::for_each<
            boost::mpl::list<
                ChangeAddress,
                SetCurveMin,
                SetCurveMax,
                InitAutomation
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
