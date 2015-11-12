#include "ScenarioInspectorFactory.hpp"
#include "ScenarioInspectorWidget.hpp"
#include <Scenario/Process/ScenarioModel.hpp>

//using namespace iscore;

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    return new ScenarioInspectorWidget{
        static_cast<const ScenarioModel&>(sourceElement),
                doc,
                parent};
}
