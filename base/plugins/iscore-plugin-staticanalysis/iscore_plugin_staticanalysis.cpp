#include <StaticAnalysis/ScenarioVisitor.hpp>
#include "iscore_plugin_staticanalysis.hpp"

namespace iscore {

}  // namespace iscore

iscore_addon_staticanalysis::iscore_addon_staticanalysis() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_addon_staticanalysis::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    return new stal::ApplicationPlugin{app};
}
