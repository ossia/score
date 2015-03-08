#include "IScoreCohesion.hpp"
#include "IScoreCohesionControl.hpp"
IScoreCohesion::IScoreCohesion() :
    QObject {}
{
}

iscore::PluginControlInterface* IScoreCohesion::control_make()
{
    return new IScoreCohesionControl {nullptr};
}


