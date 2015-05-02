#include "IScoreCohesion.hpp"
#include "IScoreCohesionControl.hpp"
IScoreCohesion::IScoreCohesion() :
    QObject {}
{
}

iscore::PluginControlInterface* IScoreCohesion::control()
{
    return new IScoreCohesionControl {nullptr};
}


