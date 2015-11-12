#pragma once
#include <string>
#include <functional>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

class StringFactoryKeyBase
{
        friend struct std::hash<StringFactoryKeyBase>;
        friend bool operator==(const StringFactoryKeyBase& lhs, const StringFactoryKeyBase& rhs) {
            return lhs.impl == rhs.impl;
        }

        friend bool operator<(const StringFactoryKeyBase& lhs, const StringFactoryKeyBase& rhs) {
            return lhs.impl < rhs.impl;
        }

    public:
        StringFactoryKeyBase() = default;
        StringFactoryKeyBase(const char* str): impl{str} {}
        StringFactoryKeyBase(const std::string& str): impl{str} {}
        StringFactoryKeyBase(std::string&& str): impl{std::move(str)} {}

    protected:
        std::string impl;
};



namespace std
{
template<>
struct hash<StringFactoryKeyBase>
{
        std::size_t operator()(const StringFactoryKeyBase& kagi) const noexcept
        {
            return std::hash<std::string>()(kagi.impl);
        }
};
}

template<typename Tag>
class StringFactoryKey : StringFactoryKeyBase
{
        using this_type = StringFactoryKey<Tag>;

        template<typename U>
        friend void Visitor<Reader< DataStream >>::readFrom (const StringFactoryKey<U> &);
        template<typename U>
        friend void Visitor<Writer< DataStream >>::writeTo (StringFactoryKey<U> &);
        template<typename U>
        friend void Visitor<Reader< JSONValue >>::readFrom (const StringFactoryKey<U> &);
        template<typename U>
        friend void Visitor<Writer< JSONValue >>::writeTo (StringFactoryKey<U> &);

        friend struct std::hash<this_type>;
        friend bool operator==(const this_type& lhs, const this_type& rhs) {
            return static_cast<const StringFactoryKeyBase&>(lhs) == static_cast<const StringFactoryKeyBase&>(rhs);
        }

        friend bool operator<(const this_type& lhs, const this_type& rhs) {
            return static_cast<const StringFactoryKeyBase&>(lhs) < static_cast<const StringFactoryKeyBase&>(rhs);
        }

    public:
        using StringFactoryKeyBase::StringFactoryKeyBase;
};

namespace std
{
template<typename T>
struct hash<StringFactoryKey<T>>
{
        std::size_t operator()(const StringFactoryKey<T>& kagi) const noexcept
        { return std::hash<StringFactoryKeyBase>()(static_cast<const StringFactoryKeyBase&>(kagi)); }
};
}

template<typename T>
void Visitor<Reader<DataStream>>::readFrom(const StringFactoryKey<T>& key)
{
    m_stream << key.impl;
}

template<typename T>
void Visitor<Writer<DataStream>>::writeTo(StringFactoryKey<T>& key)
{
    m_stream >> key.impl;
}

template<typename T>
void Visitor<Reader<JSONValue>>::readFrom(const StringFactoryKey<T>& key)
{
    val = QString::fromStdString(key.impl);
}

template<typename T>
void Visitor<Writer<JSONValue>>::writeTo(StringFactoryKey<T>& key)
{
    key.impl = val.toString().toStdString();
}
