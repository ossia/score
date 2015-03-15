#include "ScenarioInspectorFactory.hpp"
#include "ScenarioInspectorWidget.hpp"
#include "Process/ScenarioModel.hpp"

//using namespace iscore;

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget(QObject* sourceElement)
{
    return new ScenarioInspectorWidget(static_cast<ScenarioModel*>(sourceElement));
}
