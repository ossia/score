#include "LoopInspectorFactory.hpp"
#include "LoopInspectorWidget.hpp"
#include <Loop/LoopProcessModel.hpp>

//using namespace iscore;

LoopInspectorFactory::LoopInspectorFactory() :
    InspectorWidgetFactory {}
{

}

LoopInspectorFactory::~LoopInspectorFactory()
{

}

InspectorWidgetBase* LoopInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    return new LoopInspectorWidget{
                safe_cast<const LoopProcessModel&>(sourceElement),
                doc,
                parent};
}
