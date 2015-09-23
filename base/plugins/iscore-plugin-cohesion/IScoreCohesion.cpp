#include "IScoreCohesion.hpp"
#include "IScoreCohesionControl.hpp"
iscore_plugin_cohesion::iscore_plugin_cohesion() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_cohesion::make_control(iscore::Presenter* pres)
{
    return new IScoreCohesionControl {pres};
}

QStringList iscore_plugin_cohesion::required() const
{
    return {"Scenario"};
}
