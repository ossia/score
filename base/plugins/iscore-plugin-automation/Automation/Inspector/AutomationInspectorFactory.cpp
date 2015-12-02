#include <QString>

#include <Automation/AutomationProcessMetadata.hpp>
#include "AutomationInspectorFactory.hpp"
#include "AutomationInspectorWidget.hpp"
#include <Automation/AutomationModel.hpp>
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

//using namespace iscore;

InspectorWidgetBase* AutomationInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new AutomationInspectorWidget{
                safe_cast<const AutomationModel&>(sourceElement),
                doc,
                parent};
}

const QList<QString>& AutomationInspectorFactory::key_impl() const
{
    static const QList<QString> lst{AutomationProcessMetadata::processObjectName()};
    return lst;
}
