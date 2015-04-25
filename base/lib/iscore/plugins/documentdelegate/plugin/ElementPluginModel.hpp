#pragma once
#include <QObject>
#include <iscore/serialization/VisitorInterface.hpp>

namespace iscore
{
class ElementPluginModel : public QObject
{
    public:
        // Note : subclasses should take an element as a parameter in order to be able to do useful things.
        using QObject::QObject;

        virtual ElementPluginModel* clone(const QObject* element, QObject* parent) const = 0;

        virtual QString plugin() const = 0;

        virtual void serialize(const VisitorVariant&) const = 0;
};

}
