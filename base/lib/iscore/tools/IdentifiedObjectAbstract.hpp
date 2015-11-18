#pragma once
#include <iscore/tools/NamedObject.hpp>
class IdentifiedObjectAbstract : public NamedObject
{
        Q_OBJECT
    public:
        virtual int32_t id_val() const = 0;
        virtual ~IdentifiedObjectAbstract();

    signals:
        void identified_object_destroyed(IdentifiedObjectAbstract*);

    protected:
        using NamedObject::NamedObject;
};
