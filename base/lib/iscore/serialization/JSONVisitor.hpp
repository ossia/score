#pragma once
#include <iscore/serialization/JSONValueVisitor.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>

template<typename T>
T fromJsonObject(QJsonObject&& json);

class JSONObject;
template<> class Visitor<Reader<JSONObject>>;
template<> class Visitor<Writer<JSONObject>>;

class JSONObject
{
    public:
        using Serializer = Visitor<Reader<JSONObject>>;
        using Deserializer = Visitor<Writer<JSONObject>>;
        static constexpr SerializationIdentifier type()
        {
            return 1;
        }
};

template<>
class Visitor<Reader<JSONObject>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        Visitor<Reader<JSONObject>>() = default;
        Visitor<Reader<JSONObject>>(const Visitor<Reader<JSONObject>>&) = delete;
        Visitor<Reader<JSONObject>>& operator=(const Visitor<Reader<JSONObject>>&) = delete;

        VisitorVariant toVariant() { return {*this, JSONObject::type()}; }
        template<typename T>
        void readFrom(const T&);

        template<typename T>
        void readFrom(const IdentifiedObject<T>& obj)
        {
            readFrom(static_cast<const NamedObject&>(obj));
            readFrom(obj.id().val());
        }

        QJsonObject m_obj;
};

template<>
class Visitor<Writer<JSONObject>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, JSONObject::type()}; }

        Visitor<Writer<JSONObject>>() = default;
        Visitor<Writer<JSONObject>>(const Visitor<Reader<JSONObject>>&) = delete;
        Visitor<Writer<JSONObject>>& operator=(const Visitor<Writer<JSONObject>>&) = delete;

        Visitor<Writer<JSONObject>> (const QJsonObject& obj) :
                               m_obj {obj}
        {}

        Visitor<Writer<JSONObject>> (QJsonObject&& obj) :
                               m_obj {std::move(obj) }
        {}

        template<typename T>
        void writeTo(T&);

        template<typename T>
        T writeTo()
        {
            T val;
            writeTo(val);
            return val;
        }

        template<typename T>
        void writeTo(IdentifiedObject<T>& obj)
        {
            typename id_type<T>::value_type id_impl;
            writeTo(id_impl);
            id_type<T> id;
            id.setVal(std::move(id_impl));
            obj.setId(std::move(id));
        }

        QJsonObject m_obj;
};

template<typename T>
QJsonObject toJsonObject(const T& obj)
{
    Visitor<Reader<JSONObject>> reader;
    reader.readFrom(obj);

    return reader.m_obj;
}

template<typename T>
void fromJsonObject(QJsonObject&& json, T& obj)
{
    Visitor<Writer<JSONObject>> writer {json};
    writer.writeTo(obj);
}

template<typename T>
T fromJsonObject(QJsonObject&& json)
{
    T obj;
    Visitor<Writer<JSONObject>> writer {json};
    writer.writeTo(obj);

    return obj;
}

template<template<typename U> class Container>
QJsonArray toJsonArray(const Container<int>& array)
{
    QJsonArray arr;

    for(const auto& elt : array)
    {
        arr.append(elt);
    }

    return arr;
}

template<class Container>
QJsonArray toJsonArray_sub(const Container& array, std::false_type)
{
    QJsonArray arr;

    for(const auto& elt : array)
    {
        arr.append(toJsonObject(elt));
    }

    return arr;
}

template<class Container>
QJsonArray toJsonArray_sub(const Container& array, std::true_type)
{
    QJsonArray arr;

    for(const auto& elt : array)
    {
        arr.append(toJsonObject(*elt));
    }

    return arr;
}

template<class Container>
QJsonArray toJsonArray(const Container& array)
{
    return toJsonArray_sub(array, std::is_pointer<return_type_of_iterator<Container>>());
}

template<template<typename U> class T, typename V>
QJsonArray toJsonArray(const T<id_type<V>>& array)
{
    QJsonArray arr;

    for(const auto& elt : array)
    {
        arr.append(toJsonValue(elt));
    }

    return arr;
}

template<typename Value>
QJsonArray toJsonMap(const QMap<double, Value>& map)
{
    QJsonArray arr;

    for(const auto& key : map.keys())
    {
        QJsonObject obj;
        obj["k"] = key;
        obj["v"] = map[key];
        arr.append(obj);
    }

    return arr;
}

template<typename Key, typename Value>
QJsonArray toJsonMap(const QMap<Key, Value>& map)
{
    QJsonArray arr;

    for(const auto& key : map.keys())
    {
        QJsonObject obj;
        obj["k"] = *key.val();
        obj["v"] = map[key];
        arr.append(obj);
    }

    return arr;
}


template<typename Key, typename Value,
         std::enable_if_t<std::is_same<bool, Value>::value>* = nullptr>
QMap<Key, Value> fromJsonMap(const QJsonArray& array)
{
    QMap<Key, Value> map;

    for(const auto& value : array)
    {
        QJsonObject obj = value.toObject();
        map[Key {obj["k"].toInt()}] = obj["v"].toBool();
    }

    return map;
}

template<typename Key>
QMap<Key, double> fromJsonMap(const QJsonArray& array)
{
    QMap<Key, double> map;

    for(const auto& value : array)
    {
        QJsonObject obj = value.toObject();
        map[Key {obj["k"].toInt() }] = obj["v"].toDouble();
    }

    return map;
}

template<>
inline QMap<int32_t, double> fromJsonMap(const QJsonArray& array)
{
    QMap<int32_t, double> map;

    for(const auto& value : array)
    {
        QJsonObject obj = value.toObject();
        map[obj["k"].toInt()] = obj["v"].toDouble();
    }

    return map;
}
template<>
inline QMap<double, double> fromJsonMap(const QJsonArray& array)
{
    QMap<double, double> map;

    for(const auto& value : array)
    {
        QJsonObject obj = value.toObject();
        map[obj["k"].toDouble()] = obj["v"].toDouble();
    }

    return map;
}

template<template<typename U> class Container>
void fromJsonArray(QJsonArray&& json_arr, Container<int>& arr)
{
    for(const auto& elt : json_arr)
    {
        arr.push_back(elt.toInt());
    }
}

template<template<typename U> class Container>
void fromJsonArray(QJsonArray&& json_arr, Container<QString>& arr)
{
    for(const auto& elt : json_arr)
    {
        arr.push_back(elt.toString());
    }
}



template<template<typename U> class Container, typename T>
void fromJsonArray(QJsonArray&& json_arr, Container<T>& arr)
{
    for(const auto& elt : json_arr)
    {
        T obj;
        fromJsonObject(elt.toObject(), obj);
        arr.push_back(obj);
    }
}


Q_DECLARE_METATYPE(Visitor<Reader<JSONObject>>*)
Q_DECLARE_METATYPE(Visitor<Writer<JSONObject>>*)

