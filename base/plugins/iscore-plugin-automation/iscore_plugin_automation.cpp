#include "iscore_plugin_automation.hpp"

#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"

#if defined(ISCORE_LIB_INSPECTOR)
#include "Automation/Inspector/AutomationInspectorFactory.hpp"
#include "Automation/Inspector/AutomationStateInspectorFactory.hpp"
#endif

#include "Automation/Commands/ChangeAddress.hpp"
#include "Automation/Commands/SetCurveMin.hpp"
#include "Automation/Commands/SetCurveMax.hpp"
#include "Automation/Commands/InitAutomation.hpp"

iscore_plugin_automation::iscore_plugin_automation() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_automation::make_control(
        iscore::Presenter* pres)
{
    return new AutomationControl{pres};
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
