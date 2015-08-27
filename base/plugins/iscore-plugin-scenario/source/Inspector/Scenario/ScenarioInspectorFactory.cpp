#include "ScenarioInspectorFactory.hpp"
#include "ScenarioInspectorWidget.hpp"
#include "Process/ScenarioModel.hpp"

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


QList<QString> ScenarioInspectorFactory::correspondingObjectsNames() const
{
    return {"Scenario"};
}
