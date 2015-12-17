#pragma once
#include <QObject>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/Todo.hpp>
////////////////////////////////////////////////
// This file contains utility algorithms & classes that can be used
// everywhere (core, plugins...)
////////////////////////////////////////////////
class ISCORE_LIB_BASE_EXPORT NamedObject : public QObject
{
    public:
        template<typename... Args>
        NamedObject(const QString& name, QObject* parent, Args&& ... args) :
            QObject {std::forward<Args> (args)...}
        {
            QObject::setObjectName(name);
            QObject::setParent(parent);
        }

        template<typename ReaderImpl, typename... Args>
        NamedObject(Deserializer<ReaderImpl>& v, QObject* parent, Args&& ... args) :
            QObject {std::forward<Args> (args)...}
        {
            v.writeTo(*this);
            QObject::setParent(parent);
        }

        virtual ~NamedObject();
};


inline void debug_parentHierarchy(QObject* obj)
{
    while(obj)
    {
        qDebug() << obj->objectName();
        obj = obj->parent();
    }
}
