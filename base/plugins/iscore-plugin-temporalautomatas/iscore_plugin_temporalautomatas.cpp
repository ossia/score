#include "iscore_plugin_temporalautomatas.hpp"

#include "ScenarioVisitor.hpp"
iscore_plugin_temporalautomatas::iscore_plugin_temporalautomatas() :
    QObject {}
{
}

iscore::PluginControlInterface*iscore_plugin_temporalautomatas::make_control(iscore::Application& app)
{
    return new TemporalAutomatas::ApplicationPlugin{app};
}
