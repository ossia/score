#pragma once

#include <qobject.h>

struct VisitorVariant;

namespace iscore
{
using ElementPluginModelType = int;
class ElementPluginModel : public QObject
{
    protected:
        // Note : subclasses should take an element as a parameter
        // in order to be able to do useful things.
        using QObject::QObject;

    public:
        virtual ~ElementPluginModel();
        virtual ElementPluginModel* clone(
                const QObject* element,
                QObject* parent) const = 0;

        virtual ElementPluginModelType elementPluginId() const = 0;

        virtual void serialize(const VisitorVariant&) const = 0;
};

}
