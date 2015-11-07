#include "InspectorWidgetFactoryInterface.hpp"

InspectorWidgetFactory::~InspectorWidgetFactory()
{

}

QString InspectorWidgetFactory::factoryName()
{
    return "Inspector";
}
