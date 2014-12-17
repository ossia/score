#include "CurvePlugin.hpp"
#include <interface/customfactory/CustomFactoryInterface.hpp>

#include <ProcessInterface/ProcessViewInterface.hpp>
#include "plugincurve/include/plugincurve.hpp"


CurvePlugin::CurvePlugin() :
	QObject{}
{
	setObjectName("CurvePlugin");
	qDebug(Q_FUNC_INFO);
}

QVector<iscore::FactoryInterface*> CurvePlugin::factories_make (QString factoryName)
{
	qDebug() << "yeah";
	if (factoryName == "Process")
	{
		return {new PluginCurveFactory};
	}

	return {};
}
