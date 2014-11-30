#include <ScenarioPlugin.hpp>
#include <QStringList>
#include <Process/ScenarioProcessFactory.hpp>

ScenarioPlugin::ScenarioPlugin():
	QObject{},
	iscore::Autoconnect_QtInterface{},
	iscore::DocumentDelegateFactoryInterface_QtInterface{},
	iscore::ProcessFactoryInterface_QtInterface{}
{
	setObjectName("ScenarioPluginRework");
}


QList<iscore::Autoconnect> ScenarioPlugin::autoconnect_list() const
{
	return
	{
	//	{{iscore::Autoconnect::ObjectRepresentationType::QObjectName, "BaseElementPresenter", SIGNAL(submitCommand(iscore::SerializableCommand*))},
	//	 {iscore::Autoconnect::ObjectRepresentationType::QObjectName, "Presenter", SLOT(applyCommand(iscore::SerializableCommand*))}}
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



