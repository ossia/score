#include "InspectorWidgetFactoryInterface.hpp"

InspectorWidgetFactory::~InspectorWidgetFactory()
{

}

bool InspectorWidgetFactory::matches(const QString& objectName)
{
    return key_impl().contains(objectName);
}
