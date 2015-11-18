#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

//using namespace iscore;

InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    auto& timeNode = static_cast<const TimeNodeModel&>(sourceElement);
    return new TimeNodeInspectorWidget{timeNode, doc, parent};
}

const QList<QString>& TimeNodeInspectorFactory::key_impl() const
{
    static const QList<QString> list{"TimeNodeModel"};
    return list;
}
