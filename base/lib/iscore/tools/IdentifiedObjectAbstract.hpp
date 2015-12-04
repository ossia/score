#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <sys/types.h>

class ISCORE_LIB_BASE_EXPORT IdentifiedObjectAbstract : public NamedObject
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
