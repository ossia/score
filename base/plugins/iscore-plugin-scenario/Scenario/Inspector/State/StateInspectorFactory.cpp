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
        QList<const QObject*> sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parentWidget) const
{
    return new StateInspectorWidget{
        static_cast<const StateModel&>(*sourceElements.first()),
                doc,
                parentWidget};
}

bool StateInspectorFactory::matches(QList<const QObject*> objects) const
{
    return dynamic_cast<const StateModel*>(objects.first());
}
}
