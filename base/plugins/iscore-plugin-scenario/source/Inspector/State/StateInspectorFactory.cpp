#include "StateInspectorFactory.hpp"
#include "StateInspectorWidget.hpp"

#include <Document/State/DisplayedStateModel.hpp>

InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QObject* sourceElement,
        QWidget* parentWidget)
{
    return new StateInspectorWidget{static_cast<const StateModel*>(sourceElement),
                parentWidget};
}

QList<QString> StateInspectorFactory::correspondingObjectsNames() const
{
    return {"StateModel"};
}
