#include <Scenario/Document/State/StateModel.hpp>
#include <QString>

#include "StateInspectorFactory.hpp"
#include "StateInspectorWidget.hpp"

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
Inspector::InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parentWidget) const
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
}
