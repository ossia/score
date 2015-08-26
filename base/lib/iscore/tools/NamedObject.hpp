#pragma once
#include <QObject>
#include "iscore/serialization/VisitorInterface.hpp"
#include "Todo.hpp"
////////////////////////////////////////////////
// This file contains utility algorithms & classes that can be used
// everywhere (core, plugins...)
////////////////////////////////////////////////
template<typename QType>
class NamedType : public QType
{
    public:
        template<typename... Args>
        NamedType(const QString& name, QObject* parent, Args&& ... args) :
            QType {std::forward<Args> (args)...}
        {
            QType::setObjectName(name);
            QType::setParent(parent);
        }

        template<typename ReaderImpl, typename... Args>
        NamedType(Deserializer<ReaderImpl>& v, QObject* parent, Args&& ... args) :
            QType {std::forward<Args> (args)...}
        {
            v.writeTo(*this);
            QType::setParent(parent);
        }
};


using NamedObject = NamedType<QObject>;


inline void debug_parentHierarchy(QObject* obj)
{
    while(obj)
    {
        qDebug() << obj->objectName();
        obj = obj->parent();
    }
}

class IdentifiedObjectAbstract : public NamedObject
{
    public:
        virtual int32_t id_val() const = 0;

    protected:
        using NamedObject::NamedObject;
};
