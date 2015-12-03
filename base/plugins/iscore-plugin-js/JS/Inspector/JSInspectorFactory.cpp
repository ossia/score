#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "JSInspectorFactory.hpp"
#include "JS/JSProcessModel.hpp"
#include "JSInspectorWidget.hpp"

class InspectorWidgetBase;
class JSProcessModel;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

//using namespace iscore;

JSInspectorFactory::JSInspectorFactory() :
    InspectorWidgetFactory {}
{

}

JSInspectorFactory::~JSInspectorFactory()
{

}

InspectorWidgetBase* JSInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new JSInspectorWidget{
                safe_cast<const JSProcessModel&>(sourceElement),
                doc,
                parent};
}
