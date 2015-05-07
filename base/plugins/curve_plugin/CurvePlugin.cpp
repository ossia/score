#include "CurvePlugin.hpp"

#include "Inspector/AutomationInspectorFactory.hpp"
#include "Inspector/AutomationStateInspectorFactory.hpp"
#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"

CurvePlugin::CurvePlugin() :
    QObject {}
{
}

iscore::PluginControlInterface* CurvePlugin::control()
{
    return new AutomationControl{nullptr};
}

QVector<iscore::FactoryInterface*> CurvePlugin::factories(const QString& factoryName)
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
