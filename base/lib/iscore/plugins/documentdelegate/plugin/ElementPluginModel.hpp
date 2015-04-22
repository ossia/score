#pragma once
#include <QObject>
#include <iscore/serialization/VisitorInterface.hpp>

namespace iscore
{
class ElementPluginModel : public QObject
{
    public:
        using QObject::QObject;

        virtual ElementPluginModel* clone(QObject* parent) const = 0;

        virtual QString plugin() const = 0;

        virtual void serialize(SerializationIdentifier identifier,
                               void* data) const = 0;
};

}
