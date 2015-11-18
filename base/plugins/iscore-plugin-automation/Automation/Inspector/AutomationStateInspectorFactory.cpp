#include "AutomationStateInspectorFactory.hpp"
#include "AutomationStateInspector.hpp"
#include <Automation/State/AutomationState.hpp>

AutomationStateInspectorFactory::AutomationStateInspectorFactory() :
    InspectorWidgetFactory {}
{

}

InspectorWidgetBase* AutomationStateInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    return new AutomationStateInspector{
                safe_cast<const AutomationState&>(sourceElement),
                doc,
                parent};
}

const QList<QString>&AutomationStateInspectorFactory::key_impl() const
{
    static const QList<QString> lst{"AutomationState"};
    return lst;
}
