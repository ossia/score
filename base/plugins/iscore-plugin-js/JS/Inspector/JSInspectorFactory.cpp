#include "JSInspectorFactory.hpp"
#include "JSInspectorWidget.hpp"
#include <JS/JSProcessModel.hpp>

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
        iscore::Document& doc,
        QWidget* parent)
{
    return new JSInspectorWidget{
                safe_cast<const JSProcessModel&>(sourceElement),
                doc,
                parent};
}
