#include <QList>

#include "InspectorWidgetFactoryInterface.hpp"

InspectorWidgetFactory::~InspectorWidgetFactory()
{

}

bool InspectorWidgetFactory::matches(const QString& objectName) const
{
    return key_impl().contains(objectName);
}
