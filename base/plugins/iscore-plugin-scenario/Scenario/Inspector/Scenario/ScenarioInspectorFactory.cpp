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
        static_cast<const Scenario::ScenarioModel&>(sourceElement),
                doc,
                parent};
}

const QList<QString>&ScenarioInspectorFactory::key_impl() const
{
    static const QList<QString> list{ScenarioProcessMetadata::processObjectName()};
    return list;
}
