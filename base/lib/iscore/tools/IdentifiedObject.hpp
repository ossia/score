#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/utilsCPP11.hpp>

// TODO ModelObject with path() and a pointer cache.
template<typename model>
/**
 * @brief The IdentifiedObject class
 *
 * An object with an unique identifier. This identifier
 * is used to find objects in a path.
 *
 * A class should only have a single child of the same type with a given identifier
 * since QObject::findChild is used.
 */
class IdentifiedObject : public IdentifiedObjectAbstract
{
    public:
        using model_type = model;
        template<typename... Args>
        IdentifiedObject(const Id<model>& id,
                         Args&& ... args) :
            IdentifiedObjectAbstract {std::forward<Args> (args)...},
            m_id {id}
        {
        }

        template<typename ReaderImpl, typename... Args>
        IdentifiedObject(Deserializer<ReaderImpl>& v,
                         Args&& ... args) :
            IdentifiedObjectAbstract {v, std::forward<Args> (args)...}
        {
            v.writeTo(*this);
        }

        const Id<model>& id() const
        {
            return m_id;
        }

        virtual int32_t id_val() const override
        {
            return *m_id.val();
        }

        void setId(Id<model>&& id)
        {
            m_id = id;
        }

    private:
        Id<model> m_id {};
};


template<typename model>
std::size_t hash_value(const Id<model>& id)
{
    ISCORE_ASSERT(bool(id));

    return *id.val();
}

template<typename T, typename U>
bool operator==(const T* obj, const Id<U>& id)
{
    return obj->id() == id;
}

template<typename T, typename U,std::enable_if_t<! std::is_pointer<std::decay_t<T>>::value>* = nullptr>
bool operator==(const T& obj, const Id<U>& id)
{
    return obj.id() == id;
}
