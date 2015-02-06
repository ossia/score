#include "CurvePlugin.hpp"

#include "Inspector/AutomationInspectorFactory.hpp"
#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"
#include <QVector>

CurvePlugin::CurvePlugin() :
	QObject{}
{
	setObjectName("CurvePlugin");
}

QStringList CurvePlugin::control_list() const
{
	return {"AutomationControl"};
}

iscore::PluginControlInterface*CurvePlugin::control_make(QString name)
{
	if(name == "AutomationControl")
	{
		return new AutomationControl{nullptr};
	}
}

QVector<iscore::FactoryInterface*> CurvePlugin::factories_make (QString factoryName)
{
	if (factoryName == "Process")
	{
		return {new AutomationFactory};
	}

	if(factoryName == "Inspector")
	{
		return {new AutomationInspectorFactory};
	}

	return {};
}
