#include <QString>

#include "AutomationStateInspector.hpp"
#include "AutomationStateInspectorFactory.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Automation/State/AutomationState.hpp>
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Automation
{
StateInspectorFactory::StateInspectorFactory() :
    InspectorWidgetFactory {}
{

}

Inspector::InspectorWidgetBase* StateInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new StateInspectorWidget{
                safe_cast<const ProcessState&>(sourceElement),
                doc,
                parent};
}

const QList<QString>& StateInspectorFactory::key_impl() const
{
    static const QList<QString> lst{"AutomationState"};
    return lst;
}

bool StateInspectorFactory::matches(const QObject& object) const
{
    return dynamic_cast<const ProcessState*>(&object);
}
}
