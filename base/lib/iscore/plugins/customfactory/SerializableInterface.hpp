#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore_lib_base_export.h>
class DataStream;
class JSONObject;
namespace iscore
{
using uuid_t = boost::uuids::uuid;
class ISCORE_LIB_BASE_EXPORT SerializableInterface
{
    public:
        SerializableInterface();

        virtual ~SerializableInterface();
        virtual uuid_t uuid() const = 0;
        void serialize(const VisitorVariant& vis) const;

    protected:
        virtual void serialize_impl(const VisitorVariant& vis) const;
};
}

#define ISCORE_RETURN_UUID(Uuid) \
    static const auto id = boost::uuids::string_generator{}(#Uuid); \
    return id;


template<typename Tag>
class ISCORE_LIB_BASE_EXPORT UuidKey : iscore::uuid_t
{
        using this_type = UuidKey<Tag>;

        friend struct std::hash<this_type>;
        friend bool operator==(const this_type& lhs, const this_type& rhs) {
            return static_cast<const iscore::uuid_t&>(lhs) == static_cast<const iscore::uuid_t&>(rhs);
        }

        friend bool operator<(const this_type& lhs, const this_type& rhs) {
            return static_cast<const iscore::uuid_t&>(lhs) < static_cast<const iscore::uuid_t&>(rhs);
        }

    public:
        using iscore::uuid_t::uuid_t;
};

namespace std
{
template<typename T>
struct hash<UuidKey<T>>
{
        std::size_t operator()(const UuidKey<T>& kagi) const noexcept
        { return boost::hash<iscore::uuid_t>()(static_cast<const iscore::uuid_t&>(kagi)); }
};
}


template<typename Type>
UuidKey<Type> deserialize_key(Deserializer<JSONObject>& des)
{
    return fromJsonValue<boost::uuids::uuid>(des.m_obj["uuid"]);

}

template<typename Type>
UuidKey<Type> deserialize_key(Deserializer<DataStream>& des)
{
    boost::uuids::uuid uid;
    des.writeTo(uid);
    return uid;

}

template<typename FactoryList_T, typename Deserializer, typename... Args>
auto deserialize_interface(
        const FactoryList_T& factories,
        Deserializer& des,
        Args&&... args)
{
    // Deserialize the interface identifier
    auto k = deserialize_key<typename FactoryList_T::factory_type>(des);

    // Get the factory
    auto concrete_factory = factories.get(k);
    if(!concrete_factory)
        return nullptr;

    // Create the object
    return concrete_factory->load(des.toVariant(), std::forward<Args>(args)...);
}

