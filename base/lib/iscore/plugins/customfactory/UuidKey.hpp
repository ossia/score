#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/functional/hash.hpp>
#include <iscore_lib_base_export.h>

#include <iscore/serialization/DataStreamVisitor.hpp>
class JSONObject;

namespace iscore
{
using uuid_t = boost::uuids::uuid;
}

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
        UuidKey() = default;
        UuidKey(const UuidKey& other) = default;
        UuidKey(UuidKey&& other) = default;
        UuidKey& operator=(const UuidKey& other) = default;
        UuidKey& operator=(UuidKey&& other) = default;

        UuidKey(iscore::uuid_t other):
            iscore::uuid_t(other)
        {

        }

        UuidKey(const char* txt):
            iscore::uuid_t{boost::uuids::string_generator{}(txt)}
        {

        }

        template<int N>
        UuidKey(const char txt[N]):
            iscore::uuid_t{boost::uuids::string_generator{}(txt)}
        {

        }

        const iscore::uuid_t& impl() const { return *this; }
        iscore::uuid_t& impl() { return *this; }
};

namespace std
{
template<typename T>
struct hash<UuidKey<T>>
{
        std::size_t operator()(const UuidKey<T>& kagi) const noexcept
        { return boost::hash<boost::uuids::uuid>()(static_cast<const iscore::uuid_t&>(kagi)); }
};
}

template<typename U>
struct TSerializer<DataStream, UuidKey<U>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const UuidKey<U>& uid)
        {
            for(auto val : uid.impl().data)
                s.stream() << val;
        }

        static void writeTo(
                DataStream::Deserializer& s,
                UuidKey<U>& uid)
        {
            for(auto& val : uid.impl().data)
                s.stream() >> val;
        }
};


#include <iscore/serialization/JSONValueVisitor.hpp>
template<typename U>
struct TSerializer<JSONValue, UuidKey<U>>
{
        static void readFrom(
                JSONValue::Serializer& s,
                const UuidKey<U>& uid)
        {
            s.readFrom(uid.impl());
        }

        static void writeTo(
                JSONValue::Deserializer& s,
                UuidKey<U>& uid)
        {
            s.writeTo(uid.impl());
        }
};
