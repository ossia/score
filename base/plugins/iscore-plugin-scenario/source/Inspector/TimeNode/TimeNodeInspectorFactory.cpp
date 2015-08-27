#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

#include <Document/TimeNode/TimeNodeModel.hpp>

//using namespace iscore;

InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget(
        const QObject& sourceElement,
        QWidget* parent)
{
    auto& timeNode = static_cast<const TimeNodeModel&>(sourceElement);
    return new TimeNodeInspectorWidget{timeNode, parent};
}

QList<QString> TimeNodeInspectorFactory::correspondingObjectsNames() const
{
    return {"TimeNodeModel"};
}
