#include "IScoreCohesion.hpp"
#include "IScoreCohesionControl.hpp"
IScoreCohesion::IScoreCohesion() :
    QObject {}
{
}

QStringList IScoreCohesion::control_list() const
{
    return {"IScoreCohesionControl"};
}

iscore::PluginControlInterface* IScoreCohesion::control_make(QString name)
{
    return new IScoreCohesionControl {nullptr};
}


