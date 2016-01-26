#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QJsonValue>
#include <QJsonArray>
#include <QMap>

template<class>
class StringKey;

template<typename T>
T fromJsonObject(QJsonValue&& json);

class JSONValue;
template<> class Visitor<Reader<JSONValue>>;
template<> class Visitor<Writer<JSONValue>>;

class JSONValue
{
    public:
        using Serializer = Visitor<Reader<JSONValue>>;
        using Deserializer = Visitor<Writer<JSONValue>>;
        static constexpr SerializationIdentifier type()
        {
            return 3;
        }
};

template<>
class ISCORE_LIB_BASE_EXPORT Visitor<Reader<JSONValue>>  : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        Visitor<Reader<JSONValue>>() = default;
        Visitor<Reader<JSONValue>>(const Visitor<Reader<JSONValue>>&) = delete;
        Visitor<Reader<JSONValue>>& operator=(const Visitor<Reader<JSONValue>>&) = delete;

        VisitorVariant toVariant() { return {*this, JSONValue::type()}; }


        template<template<class...> class T, typename... Args>
        void readFrom(const T<Args...>& obj, typename std::enable_if<is_template<T<Args...>>::value, void>::type * = 0)
        {
            TSerializer<JSONValue, T<Args...>>::readFrom(*this, obj);
        }

        template<typename T, std::enable_if_t<!std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
        void readFrom(const T&);

        template<typename T, std::enable_if_t<std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
        void readFrom(const T& elt)
        {
            val = (int32_t) elt;
        }

        QJsonValue val;
};

template<>
class ISCORE_LIB_BASE_EXPORT Visitor<Writer<JSONValue>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, JSONValue::type()}; }

        Visitor<Writer<JSONValue>>() = default;
        Visitor<Writer<JSONValue>>(const Visitor<Reader<JSONValue>>&) = delete;
        Visitor<Writer<JSONValue>>& operator=(const Visitor<Writer<JSONValue>>&) = delete;

        Visitor<Writer<JSONValue>> (const QJsonValue& obj) :
                               val{obj} {}

        Visitor<Writer<JSONValue>> (QJsonValue&& obj) :
                               val{std::move(obj) }  {}


        template<
                template<class...> class T,
                typename... Args>
        void writeTo(T<Args...>& obj, typename std::enable_if<is_template<T<Args...>>::value, void>::type * = 0)
        {
            TSerializer<JSONValue, T<Args...>>::writeTo(*this, obj);
        }

        template<typename T,
                 std::enable_if_t<!std::is_enum<T>::value && !is_template<T>::value>* = nullptr >
        void writeTo(T&);
        template<typename T, std::enable_if_t<std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
        void writeTo(T& elt)
        {
            elt = static_cast<T>(val.toInt());
        }


        QJsonValue val;
};

template<>
struct TSerializer<JSONValue, boost::optional<int32_t>>
{
    static void readFrom(
            JSONValue::Serializer& s,
            const boost::optional<int32_t>& obj)
    {
        if(obj)
        {
            s.val = get(obj);
        }
        else
        {
            s.val = "none";
        }
    }

    static void writeTo(
            JSONValue::Deserializer& s,
            boost::optional<int32_t>& obj)
    {
        if(s.val.toString() == "none")
        {
            obj.reset();
        }
        else
        {
            obj = s.val.toInt();
        }
    }
};


template<typename U>
struct TSerializer<JSONValue, Id<U>>
{
    static void readFrom(
            JSONValue::Serializer& s,
            const Id<U>& obj)
    {
        s.readFrom(obj.val());
    }

    static void writeTo(
            JSONValue::Deserializer& s,
            Id<U>& obj)
    {
        typename Id<U>::value_type id_impl;
        s.writeTo(id_impl);
        obj.setVal(std::move(id_impl));
    }
};


template<typename T>
QJsonValue toJsonValue(const T& obj)
{
    Visitor<Reader<JSONValue>> reader;
    reader.readFrom(obj);

    return reader.val;
}


template<typename T>
void fromJsonValue(QJsonValue&& json, T& val)
{
    Visitor<Writer<JSONValue>> writer {json};
    writer.writeTo(val);
}

template<typename T>
void fromJsonValue(QJsonValue& json, T& val)
{
    Visitor<Writer<JSONValue>> writer {json};
    writer.writeTo(val);
}

template<typename T>
T fromJsonValue(const QJsonValue& json)
{
    T val;
    Visitor<Writer<JSONValue>> writer {json};
    writer.writeTo(val);
    return val;
}

template<typename T>
void fromJsonValue(QJsonValueRef&& json, T& val)
{
    Visitor<Writer<JSONValue>> writer {static_cast<QJsonValue>(json)};
    writer.writeTo(val);
}

template<typename T>
void fromJsonValue(const QJsonValueRef& json, T& val)
{
    Visitor<Writer<JSONValue>> writer {static_cast<const QJsonValue&>(json)};
    writer.writeTo(val);
}

template<typename T>
T fromJsonValue(QJsonValueRef&& json)
{
    T val;
    Visitor<Writer<JSONValue>> writer {static_cast<QJsonValue>(json)};
    writer.writeTo(val);
    return val;
}

template<typename T>
T fromJsonValue(const QJsonValueRef& json)
{
    T val;
    Visitor<Writer<JSONValue>> writer {static_cast<const QJsonValue&>(json)};
    writer.writeTo(val);
    return val;
}

template<template<typename U> class T, typename V>
void fromJsonValueArray(const QJsonArray&& json_arr, T<Id<V>>& arr)
{
    for(const auto& elt : json_arr)
    {
        arr.push_back(fromJsonValue<Id<V>>(elt));
    }
}


template<typename Container>
QJsonArray toJsonValueArray(const Container& c)
{
    QJsonArray arr;

    for(const auto& elt : c)
    {
        arr.push_back(toJsonValue(elt));
    }

    return arr;
}


template<typename Container>
Container fromJsonValueArray(const QJsonArray& json_arr)
{
    Container c;
    c.reserve(json_arr.size());

    for(const auto& elt : json_arr)
    {
        c.push_back(fromJsonValue<typename Container::value_type>(elt));
    }

    return c;
}
