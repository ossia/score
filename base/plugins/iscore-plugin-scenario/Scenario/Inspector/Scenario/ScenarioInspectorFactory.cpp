#include <Scenario/Process/ScenarioModel.hpp>
#include <QString>

#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include "ScenarioInspectorFactory.hpp"
#include "ScenarioInspectorWidget.hpp"

class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

//using namespace iscore;

InspectorWidgetBase* ScenarioInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent) const
{
    return new ScenarioInspectorWidget{
        static_cast<const Scenario::ScenarioModel&>(sourceElement),
                doc,
                parent};
}

const QList<QString>&ScenarioInspectorFactory::key_impl() const
{
    static const QList<QString> list{ScenarioProcessMetadata::processObjectName()};
    return list;
}
