#include "StateInspectorFactory.hpp"
#include "StateInspectorWidget.hpp"

#include <Scenario/Document/State/StateModel.hpp>

InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parentWidget)
{
    return new StateInspectorWidget{
        static_cast<const StateModel&>(sourceElement),
                doc,
                parentWidget};
}

QList<QString> StateInspectorFactory::key_impl() const
{
    return {"StateModel"};
}
