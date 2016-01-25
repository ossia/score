#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/functional/hash.hpp>
#include <iscore_lib_base_export.h>

class DataStream;
class JSONObject;

namespace iscore
{
using uuid_t = boost::uuids::uuid;
}

#define ISCORE_RETURN_UUID(Uuid) \
    static const ConcreteFactoryKey id{boost::uuids::string_generator{}(#Uuid)}; \
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
        UuidKey(iscore::uuid_t other):
            iscore::uuid_t{other}
        {

        }

        iscore::uuid_t impl() const { return *this; }
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
