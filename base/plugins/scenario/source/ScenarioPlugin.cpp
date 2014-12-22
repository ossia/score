#include <ScenarioPlugin.hpp>
#include <QStringList>
#include <Control/ScenarioControl.hpp>
#include <Process/ScenarioProcessFactory.hpp>

#include <Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Inspector/Event/EventInspectorFactory.hpp>

ScenarioPlugin::ScenarioPlugin():
	QObject{},
	iscore::Autoconnect_QtInterface{},
	iscore::PluginControlInterface_QtInterface{},
	iscore::DocumentDelegateFactoryInterface_QtInterface{},
	iscore::FactoryFamily_QtInterface{},
	iscore::FactoryInterface_QtInterface{},
	m_control{new ScenarioControl{nullptr}}
{
	setObjectName("ScenarioPlugin");
}


QList<iscore::Autoconnect> ScenarioPlugin::autoconnect_list() const
{
	return
	{
		/// Events
		{{iscore::Autoconnect::ObjectRepresentationType::QObjectName,
		  "EventModel", SIGNAL(messagesChanged())},
		 {iscore::Autoconnect::ObjectRepresentationType::QObjectName,
		  "EventInspectorWidget", SLOT(updateMessages())}}
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

QStringList ScenarioPlugin::control_list() const
{
	return {"Scenario control"};
}

iscore::PluginControlInterface* ScenarioPlugin::control_make(QString name)
{
	if(name == "Scenario control")
	{
		return m_control;
	}

	return nullptr;
}

QVector<iscore::FactoryFamily> ScenarioPlugin::factoryFamilies_make()
{
	// Todo : put these strings somewhere in an interface file.
	return {{"Process",
			 std::bind(&ProcessList::addProcess,
					   m_control->processList(),
					   std::placeholders::_1)}};
}

QVector<iscore::FactoryInterface*> ScenarioPlugin::factories_make(QString factoryName)
{
	// todo is it possible to to a better type-checking here ? :(
	if(factoryName == "Process")
	{
		return {new ScenarioProcessFactory};
	}

	if(factoryName == "Inspector")
	{
		return {new ConstraintInspectorFactory,
				new EventInspectorFactory};
	}

	return {};
}


