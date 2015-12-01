#pragma once
#include <iscore/tools/opaque/OpaqueString.hpp>
#include <iscore/tools/std/String.hpp>
#include <QDebug>
// TODO rename file.
template<typename Tag>
class StringKey : OpaqueString
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
