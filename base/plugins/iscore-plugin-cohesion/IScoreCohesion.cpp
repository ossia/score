#include "IScoreCohesion.hpp"
#include "IScoreCohesionControl.hpp"
iscore_plugin_cohesion::iscore_plugin_cohesion() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_cohesion::control()
{
    return new IScoreCohesionControl {nullptr};
}


