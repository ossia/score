#include <ScenarioPlugin.hpp>
#include <QStringList>

ScenarioPlugin::ScenarioPlugin():
	QObject{},
	DocumentDelegateFactoryInterface_QtInterface{}
{
	setObjectName("ScenarioPlugin");
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



