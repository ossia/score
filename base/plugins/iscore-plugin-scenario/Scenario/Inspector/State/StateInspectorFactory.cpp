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

bool StateInspectorFactory::matches(const QObject& object) const
{
    return dynamic_cast<const StateModel*>(&object);
}
}
