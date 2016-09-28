#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/component/Component.hpp>

namespace iscore
{
template<typename T>
class Entity :
        public IdentifiedObject<T>
{
    public:
        using IdentifiedObject<T>::IdentifiedObject;

        Entity(const Entity& other, Id<T> id, const QString& name, QObject* parent):
            IdentifiedObject<T>{std::move(id), name, parent},
            m_metadata{other.metadata()}
        {

        }

        Entity(Deserializer<DataStream>& vis, QObject* parent):
            IdentifiedObject<T>{vis, parent}
        {
            vis.writeTo(*this);
        }

        Entity(Deserializer<JSONObject>& vis, QObject* parent):
            IdentifiedObject<T>{vis, parent}
        {
            vis.writeTo(*this);
        }

        const iscore::Components& components() const { return m_components; }
        iscore::Components& components() { return m_components; }
        const iscore::ModelMetadata& metadata() const { return m_metadata; }
        iscore::ModelMetadata& metadata() { return m_metadata; }

    private:
        iscore::Components m_components;
        ModelMetadata m_metadata;
};
}


template<typename T>
struct TSerializer<DataStream, void, iscore::Entity<T>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const iscore::Entity<T>& obj)
        {
            s.readFrom(static_cast<const IdentifiedObject<T>&>(obj));
            s.readFrom(obj.metadata());
        }

        static void writeTo(
                DataStream::Deserializer& s,
                iscore::Entity<T>& obj)
        {
            s.writeTo(obj.metadata());
        }
};

template<typename T>
struct TSerializer<JSONObject, iscore::Entity<T>>
{
        static void readFrom(
                JSONObject::Serializer& s,
                const iscore::Entity<T>& obj)
        {
            s.readFrom(static_cast<const IdentifiedObject<T>&>(obj));
            s.m_obj[s.strings.Metadata] = toJsonObject(obj.metadata());
        }

        static void writeTo(
                JSONObject::Deserializer& s,
                iscore::Entity<T>& obj)
        {
            obj.metadata() = fromJsonObject<iscore::ModelMetadata>(s.m_obj[s.strings.Metadata]);
        }

};
