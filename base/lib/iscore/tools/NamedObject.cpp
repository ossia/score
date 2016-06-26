#include "NamedObject.hpp"

ISCORE_LIB_BASE_EXPORT NamedObject::~NamedObject() = default;

void debug_parentHierarchy(QObject* obj)
{
    while(obj)
    {
        qDebug() << obj->objectName();
        obj = obj->parent();
    }
}
