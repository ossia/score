#include "CurvePlugin.hpp"

//#include "Process/PluginCurveFactory.hpp"
#include "Automation/AutomationFactory.hpp"
#include <QVector>

CurvePlugin::CurvePlugin() :
	QObject{}
{
	setObjectName("CurvePlugin");
}

QVector<iscore::FactoryInterface*> CurvePlugin::factories_make (QString factoryName)
{
	if (factoryName == "Process")
	{
		return {new AutomationFactory};
	}

	return {};
}
