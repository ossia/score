#include "ScenarioInspectorFactory.hpp"
#include "ScenarioInspectorWidget.hpp"
#include "Process/ScenarioModel.hpp"

//using namespace iscore;

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget(QObject* sourceElement)
{
    return new ScenarioInspectorWidget(static_cast<ScenarioModel*>(sourceElement));

}

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget(QList<QObject*> sourceElements)
{
    // @todo make a tabbed view when there is a list.
    return new ScenarioInspectorWidget(static_cast<ScenarioModel*>(sourceElements.at(0)));
}
