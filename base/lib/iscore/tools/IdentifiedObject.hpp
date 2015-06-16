#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/utilsCPP11.hpp>

// TODO ModelObject with path() and a pointer cache.
template<typename model>
class IdentifiedObject : public IdentifiedObjectAbstract
{
    public:
        using model_type = model;
        template<typename... Args>
        IdentifiedObject(const id_type<model>& id,
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

        const id_type<model>& id() const
        {
            return m_id;
        }

        virtual int32_t id_val() const override
        {
            return *m_id.val();
        }

        void setId(id_type<model>&& id)
        {
            m_id = id;
        }

    private:
        id_type<model> m_id {};
};


template<typename model>
std::size_t hash_value(const id_type<model>& id)
{
    Q_ASSERT(bool(id));

    return *id.val();
}

template<typename T, typename U>
bool operator==(const T* obj, const id_type<U>& id)
{
    return obj->id() == id;
}
