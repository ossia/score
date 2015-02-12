#include "IScoreCohesion.hpp"
#include "IScoreCohesionControl.hpp"
IScoreCohesion::IScoreCohesion() :
	QObject{}
{
}

QList<iscore::Autoconnect> IScoreCohesion::autoconnect_list() const
{
	return
	{
	};
}

QStringList IScoreCohesion::control_list() const
{
	return {"IScoreCohesionControl"};
}

iscore::PluginControlInterface* IScoreCohesion::control_make(QString name)
{
	return new IScoreCohesionControl{nullptr};
}


