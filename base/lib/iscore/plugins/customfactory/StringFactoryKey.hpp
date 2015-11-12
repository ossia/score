#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/tools/opaque/OpaqueString.hpp>

// TODO rename file.
template<typename Tag>
class StringKey : OpaqueString
{
        using this_type = StringKey<Tag>;

        template<typename U>
        friend void Visitor<Reader< DataStream >>::readFrom (const StringKey<U> &);
        template<typename U>
        friend void Visitor<Writer< DataStream >>::writeTo (StringKey<U> &);
        template<typename U>
        friend void Visitor<Reader< JSONValue >>::readFrom (const StringKey<U> &);
        template<typename U>
        friend void Visitor<Writer< JSONValue >>::writeTo (StringKey<U> &);

        friend struct std::hash<this_type>;
        friend bool operator==(const this_type& lhs, const this_type& rhs) {
            return static_cast<const OpaqueString&>(lhs) == static_cast<const OpaqueString&>(rhs);
        }

        friend bool operator<(const this_type& lhs, const this_type& rhs) {
            return static_cast<const OpaqueString&>(lhs) < static_cast<const OpaqueString&>(rhs);
        }

    public:
        using OpaqueString::OpaqueString;
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

template<typename T>
void Visitor<Reader<DataStream>>::readFrom(const StringKey<T>& key)
{
    m_stream << key.impl;
}

template<typename T>
void Visitor<Writer<DataStream>>::writeTo(StringKey<T>& key)
{
    m_stream >> key.impl;
}

template<typename T>
void Visitor<Reader<JSONValue>>::readFrom(const StringKey<T>& key)
{
    val = QString::fromStdString(key.impl);
}

template<typename T>
void Visitor<Writer<JSONValue>>::writeTo(StringKey<T>& key)
{
    key.impl = val.toString().toStdString();
}
