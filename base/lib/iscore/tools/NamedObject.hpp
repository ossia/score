#pragma once
#include <QObject>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/Todo.hpp>
////////////////////////////////////////////////
// This file contains utility algorithms & classes that can be used
// everywhere (core, plugins...)
////////////////////////////////////////////////
class ISCORE_LIB_BASE_EXPORT NamedObject :
        public QObject
{
        Q_OBJECT
    public:
        NamedObject(const QString& name, QObject* parent)
        {
            QObject::setObjectName(name);
            QObject::setParent(parent);
        }

        template<typename ReaderImpl>
        NamedObject(Deserializer<ReaderImpl>& v, QObject* p)
        {
            v.writeTo(*this);
            QObject::setParent(p);
        }

        virtual ~NamedObject();
};


void debug_parentHierarchy(QObject* obj);
