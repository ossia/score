#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>

#include <iscore/serialization/JSONVisitor.hpp>

namespace iscore
{
struct concrete { using is_concrete_tag = std::integral_constant<bool, true>; };
template<typename T>
class ISCORE_LIB_BASE_EXPORT SerializableInterface
{
    public:
        using is_abstract_base_tag = std::integral_constant<bool, true>;

        SerializableInterface() = default;
        virtual ~SerializableInterface() = default;
        virtual UuidKey<T> concreteFactoryKey() const = 0;

        virtual void serialize_impl(const VisitorVariant& vis) const
        {

        }
};
}

template<typename Type>
Type deserialize_key(Deserializer<JSONObject>& des)
{
    return fromJsonValue<boost::uuids::uuid>(des.m_obj["uuid"]);;
}

template<typename Type>
Type deserialize_key(Deserializer<DataStream>& des)
{
    boost::uuids::uuid uid;
    des.writeTo(uid);
    return uid;
}

inline void deserialize_check(Deserializer<JSONObject>& des)
{
}

inline void deserialize_check(Deserializer<DataStream>& des)
{
    des.checkDelimiter();
}

template<typename FactoryList_T, typename Deserializer, typename... Args>
auto deserialize_interface(
        const FactoryList_T& factories,
        Deserializer& des,
        Args&&... args)
    -> typename FactoryList_T::object_type*
{
    // Deserialize the interface identifier
    auto k = deserialize_key<typename FactoryList_T::factory_type::ConcreteFactoryKey>(des);

    // Get the factory
    if(auto concrete_factory = factories.get(k))
    {
        // Create the object
        return concrete_factory->load(des.toVariant(), std::forward<Args>(args)...);
    }

    deserialize_check(des);

    return nullptr;
}
