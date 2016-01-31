#pragma once
#include <iscore/tools/opaque/OpaqueString.hpp>
#include <iscore/tools/std/String.hpp>
#include <iscore_lib_base_export.h>
#include <QDebug>
// TODO rename file.
template<typename Tag>
class ISCORE_LIB_BASE_EXPORT StringKey : OpaqueString
{
        using this_type = StringKey<Tag>;

        friend QDebug operator<< (QDebug debug, const StringKey& obj)
        {
            debug << obj.impl;
            return debug;
        }

        friend struct std::hash<this_type>;
        friend bool operator==(const this_type& lhs, const this_type& rhs) {
            return static_cast<const OpaqueString&>(lhs) == static_cast<const OpaqueString&>(rhs);
        }

        friend bool operator<(const this_type& lhs, const this_type& rhs) {
            return static_cast<const OpaqueString&>(lhs) < static_cast<const OpaqueString&>(rhs);
        }

    public:
        using OpaqueString::OpaqueString;

        auto& toString()
        { return impl; }
        auto& toString() const
        { return impl; }
};

namespace std
{
template<typename T>
struct hash<StringKey<T>>
{
        std::size_t operator()(const StringKey<T>& kagi) const noexcept
        { return std::hash<OpaqueString>()(static_cast<const OpaqueString&>(kagi)); }
};
}

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
template<typename U>
struct TSerializer<DataStream, void, StringKey<U>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const StringKey<U>& key)
        {
            s.stream() << key.toString();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                StringKey<U>& key)
        {
            s.stream() >> key.toString();
        }
};
