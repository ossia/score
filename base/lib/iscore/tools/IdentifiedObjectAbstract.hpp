#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <cinttypes>

class ISCORE_LIB_BASE_EXPORT IdentifiedObjectAbstract : public NamedObject
{
        Q_OBJECT
    public:
        virtual int32_t id_val() const = 0;
        virtual ~IdentifiedObjectAbstract();

    signals:
        // To be called by subclasses
        void identified_object_destroying(IdentifiedObjectAbstract*);

        // Will be called in the IdentifiedObjectAbstract destructor.
        void identified_object_destroyed(IdentifiedObjectAbstract*);

    protected:
        using NamedObject::NamedObject;
};
