#include <QList>

#include "InspectorWidgetFactoryInterface.hpp"

namespace Inspector
{
InspectorWidgetFactory::~InspectorWidgetFactory()
{

}

bool InspectorWidgetFactory::matches(const QString& objectName) const
{
    return key_impl().contains(objectName);
}
}
