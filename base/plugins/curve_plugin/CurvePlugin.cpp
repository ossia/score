#include "CurvePlugin.hpp"

#include <ProcessInterface/ProcessViewInterface.hpp>
#include "Process/PluginCurveFactory.hpp"


CurvePlugin::CurvePlugin() :
	QObject{}
{
	setObjectName("CurvePlugin");
}

QVector<iscore::FactoryInterface*> CurvePlugin::factories_make (QString factoryName)
{
	if (factoryName == "Process")
	{
		return {new PluginCurveFactory};
	}

	return {};
}
