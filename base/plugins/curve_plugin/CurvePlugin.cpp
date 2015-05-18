#include "CurvePlugin.hpp"

#include "Inspector/AutomationInspectorFactory.hpp"
#include "Inspector/AutomationStateInspectorFactory.hpp"
#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"

iscore_plugin_curve::iscore_plugin_curve() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_curve::control()
{
    return new AutomationControl{nullptr};
}

QVector<iscore::FactoryInterface*> iscore_plugin_curve::factories(const QString& factoryName)
{
    if(factoryName == "Process")
    {
        return {new AutomationFactory};
    }

    if(factoryName == "Inspector")
    {
        return {new AutomationInspectorFactory,
                new AutomationStateInspectorFactory};
    }

    return {};
}
