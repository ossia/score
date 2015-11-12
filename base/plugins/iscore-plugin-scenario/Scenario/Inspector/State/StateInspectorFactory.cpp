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

const QList<QString>& StateInspectorFactory::key_impl() const
{
    static const QList<QString> list{"StateModel"};
    return list;
}
