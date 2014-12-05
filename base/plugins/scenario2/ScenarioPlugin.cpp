#include <ScenarioPlugin.hpp>
#include <QStringList>
#include <Control/ScenarioControl.hpp>
#include <Process/ScenarioProcessFactory.hpp>

ScenarioPlugin::ScenarioPlugin():
	QObject{},
	iscore::Autoconnect_QtInterface{},
	iscore::PluginControlInterface_QtInterface{},
	iscore::DocumentDelegateFactoryInterface_QtInterface{},
	iscore::ProcessFactoryInterface_QtInterface{}
{
	setObjectName("ScenarioPluginRework");
}


QList<iscore::Autoconnect> ScenarioPlugin::autoconnect_list() const
{
	return
	{
	};
}


// Interfaces implementations :
QStringList ScenarioPlugin::document_list() const
{
	return {"Scenario document"};
}

#include "Document/BaseElement/ScenarioDocument.hpp"
iscore::DocumentDelegateFactoryInterface* ScenarioPlugin::document_make(QString name)
{
	if(name == QString("Scenario document"))
	{
		return new ScenarioDocument;
	}

	return nullptr;
}

QStringList ScenarioPlugin::process_list() const
{
	return {"Scenario"};
}

iscore::ProcessFactoryInterface* ScenarioPlugin::process_make(QString name)
{
	if(name == "Scenario")
	{
		return new ScenarioProcessFactory;
	}

	return nullptr;
}

QStringList ScenarioPlugin::control_list() const
{
	return {"Scenario control"};
}

iscore::PluginControlInterface*ScenarioPlugin::control_make(QString name)
{
	if(name == "Scenario control")
	{
		return new ScenarioControl{nullptr};
	}

	return nullptr;
}

QStringList ScenarioPlugin::inspectorFactory_list() const
{
	return {"IntervalInspectorFactory", "EventInspectorFactory"};
}

#include <Inspector/Interval/IntervalInspectorFactory.hpp>
#include <Inspector/Event/EventInspectorFactory.hpp>
iscore::InspectorWidgetFactoryInterface*ScenarioPlugin::inspectorFactory_make(QString str)
{
	if(str == "IntervalInspectorFactory")
	{
		return new IntervalInspectorFactory;
	}
	else if(str == "EventInspectorFactory")
	{
		return new EventInspectorFactory;
	}

	return nullptr;
}



