#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QJsonValue>
#include <QJsonArray>
#include <QVector>
#include <QMap>

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
class Visitor<Reader<JSONValue>> : public AbstractVisitor
{
    public:
        Visitor<Reader<JSONValue>>() = default;
        Visitor<Reader<JSONValue>>(const Visitor<Reader<JSONValue>>&) = delete;
        Visitor<Reader<JSONValue>>& operator=(const Visitor<Reader<JSONValue>>&) = delete;

        VisitorVariant toVariant() { return {*this, JSONValue::type()}; }

        template<typename T>
        void readFrom(const T&);

        template<typename T>
        void readFrom(const id_type<T>& obj)
        {
            readFrom(obj.val());
        }

        QJsonValue val;
};

template<>
class Visitor<Writer<JSONValue>> : public AbstractVisitor
{
    public:
        VisitorVariant toVariant() { return {*this, JSONValue::type()}; }

        Visitor<Writer<JSONValue>>() = default;
        Visitor<Writer<JSONValue>>(const Visitor<Reader<JSONValue>>&) = delete;
        Visitor<Writer<JSONValue>>& operator=(const Visitor<Writer<JSONValue>>&) = delete;

        Visitor<Writer<JSONValue>> (const QJsonValue& obj) :
                               val{obj} {}

        Visitor<Writer<JSONValue>> (QJsonValue&& obj) :
                               val{std::move(obj) }  {}

        template<typename T>
        void writeTo(T&);

        template<typename T>
        void writeTo(id_type<T>& obj)
        {
            typename id_type<T>::value_type id_impl;
            writeTo(id_impl);
            obj.setVal(std::move(id_impl));
        }

        QJsonValue val;
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
void fromJsonValueArray(const QJsonArray&& json_arr, T<id_type<V>>& arr)
{
    for(const auto& elt : json_arr)
    {
        arr.push_back(fromJsonValue<id_type<V>>(elt));
    }
}
