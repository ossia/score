#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <QString>

#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
Inspector::InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    auto& timeNode = static_cast<const TimeNodeModel&>(sourceElement);
    return new TimeNodeInspectorWidget{timeNode, doc, parent};
}

const QList<QString>& TimeNodeInspectorFactory::key_impl() const
{
    static const QList<QString> list{QString::fromStdString(TimeNodeModel::className)};
    return list;
}
}
