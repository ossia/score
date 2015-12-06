#include "ScenarioVisitor.hpp"
#include "iscore_plugin_temporalautomatas.hpp"

namespace iscore {

}  // namespace iscore

iscore_plugin_temporalautomatas::iscore_plugin_temporalautomatas() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin*iscore_plugin_temporalautomatas::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    return new TemporalAutomatas::ApplicationPlugin{app};
}
