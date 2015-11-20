#include "iscore_plugin_temporalautomatas.hpp"

#include "ScenarioVisitor.hpp"
iscore_plugin_temporalautomatas::iscore_plugin_temporalautomatas() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin*iscore_plugin_temporalautomatas::make_applicationPlugin(iscore::Application& app)
{
    return new TemporalAutomatas::ApplicationPlugin{app};
}
