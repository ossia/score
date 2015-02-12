#include "ScenarioInspectorFactory.hpp"
#include "ScenarioInspectorWidget.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

//using namespace iscore;

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget (QObject* sourceElement)
{
	return new ScenarioInspectorWidget(static_cast<ScenarioProcessSharedModel*>(sourceElement));

}

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
	// @todo make a tabbed view when there is a list.
	return new ScenarioInspectorWidget (static_cast<ScenarioProcessSharedModel*> (sourceElements.at (0) ) );
}
