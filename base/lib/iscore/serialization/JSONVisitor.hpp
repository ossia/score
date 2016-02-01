#pragma once
#include <iscore/serialization/JSONValueVisitor.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

/**
 * This file contains facilities
 * to serialize an object into a QJsonObject.
 */

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

template<class>
class TreeNode;
template<class>
class TreePath;

namespace eggs{
namespace variants {
template<class...>
class variant;
}
}

template<>
class ISCORE_LIB_BASE_EXPORT Visitor<Reader<JSONObject>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        Visitor();
        Visitor(const Visitor&) = delete;
        Visitor& operator=(const Visitor&) = delete;

        VisitorVariant toVariant() { return {*this, JSONObject::type()}; }

        template<typename T>
        static auto marshall(const T& t)
        {
            Visitor reader;
            reader.readFrom(t);
            return reader.m_obj;
        }

        template<template<class...> class T, typename... Args>
        void readFrom(
                const T<Args...>& obj,
                typename std::enable_if_t<
                    is_template<T<Args...>>::value &&
                   !is_abstract_base<T<Args...>>::value> * = 0)
        {
            TSerializer<JSONObject, T<Args...>>::readFrom(*this, obj);
        }


        template<typename T,
                 std::enable_if_t<
                     is_abstract_base<T>::value &&
                     !is_concrete<T>::value
                     >* = nullptr>
        void readFrom(const T& obj)
        {
            AbstractSerializer<JSONObject, T>::readFrom(*this, obj);
        }


        template<typename T>
        void readFrom_impl(const T&);

        template<typename T,
                 std::enable_if_t<
                     !is_template<T>::value &&
                     !is_abstract_base<T>::value>* = nullptr>
        void readFrom(const T&);

        QJsonObject m_obj;

        const iscore::ApplicationContext& context;
};

template<>
class ISCORE_LIB_BASE_EXPORT Visitor<Writer<JSONObject>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, JSONObject::type()}; }

        Visitor();
        Visitor(const Visitor&) = delete;
        Visitor& operator=(const Visitor&) = delete;

        Visitor(const QJsonObject& obj);
        Visitor(QJsonObject&& obj);

        template<typename T>
        static auto unmarshall(const QJsonObject& obj)
        {
            T data;
            Visitor wrt{obj};
            wrt.writeTo(data);
            return data;
        }

        template<typename T,
                 std::enable_if_t<!is_template<T>::value, void>* = nullptr>
        void writeTo(T&);

        template<
                template<class...> class T,
                typename... Args>
        void writeTo(T<Args...>& obj, typename std::enable_if<is_template<T<Args...>>::value, void>::type * = 0)
        {
            TSerializer<JSONObject, T<Args...>>::writeTo(*this, obj);
        }

        template<typename T>
        T writeTo()
        {
            T val;
            writeTo(val);
            return val;
        }

        const QJsonObject m_obj;
        const iscore::ApplicationContext& context;
};


template<typename T>
struct TSerializer<JSONObject, IdentifiedObject<T>>
{
        static void readFrom(
                JSONObject::Serializer& s,
                const IdentifiedObject<T>& obj)
        {
            s.readFrom(static_cast<const NamedObject&>(obj));
            s.readFrom(obj.id().val());
        }

        static void writeTo(
                JSONObject::Deserializer& s,
                IdentifiedObject<T>& obj)
        {
            typename Id<T>::value_type id_impl;
            s.writeTo(id_impl);
            Id<T> id;
            id.setVal(std::move(id_impl));
            obj.setId(std::move(id));
        }

};



template<>
struct TSerializer<JSONObject, boost::optional<int32_t>>
{
        // TODO should not be used. Save as optional json value instead.

        static void readFrom(
                JSONObject::Serializer& s,
                const boost::optional<int32_t>& obj)
        {
            if(obj)
            {
                s.m_obj["id"] = get(obj);
            }
            else
            {
                s.m_obj["id"] = "none";
            }
        }

        static void writeTo(
                JSONObject::Deserializer& s,
                boost::optional<int32_t>& obj)
        {
            if(s.m_obj["id"].toString() == "none")
            {
                obj.reset();
            }
            else
            {
                obj =s.m_obj["id"].toInt();
            }
        }
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

    for(auto elt : array)
    {
        arr.append(elt);
    }

    return arr;
}

template<template<typename U> class Container>
QJsonArray toJsonArray(const Container<unsigned int>& array)
{
    QJsonArray arr;

    for(auto elt : array)
    {
        arr.append(elt);
    }

    return arr;
}

template<template<typename U, typename V> class Container, typename V>
QJsonArray toJsonArray(const Container<unsigned int, V>& array)
{
    QJsonArray arr;

    for(auto elt : array)
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
QJsonArray toJsonArray(const T<Id<V>>& array)
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

template<template<typename U, typename V> class Container, typename T1, typename T2,
         std::enable_if_t<!std::is_arithmetic<T1>::value>* = nullptr>
void fromJsonArray(QJsonArray&& json_arr, Container<T1, T2>& arr)
{
    for(const auto& elt : json_arr)
    {
        T1 obj;
        fromJsonObject(elt.toObject(), obj);
        arr.push_back(obj);
    }
}


template<template<typename U, typename V> class Container, typename T1, typename T2,
         std::enable_if_t<std::is_integral<T1>::value>* = nullptr>
void fromJsonArray(QJsonArray&& json_arr, Container<T1, T2>& arr)
{
    int n = json_arr.size();
    arr.resize(n);
    for(int i = 0; i < n; i++)
    {
        arr[i] = json_arr[i].toInt();
    }
}

template<template<typename U, typename V> class Container, typename T1, typename T2,
         std::enable_if_t<std::is_floating_point<T1>::value>* = nullptr>
void fromJsonArray(QJsonArray&& json_arr, Container<T1, T2>& arr)
{
    int n = json_arr.size();
    arr.resize(n);
    for(int i = 0; i < n; i++)
    {
        arr[i] = json_arr[i].toDouble();
    }
}

Q_DECLARE_METATYPE(Visitor<Reader<JSONObject>>*)
Q_DECLARE_METATYPE(Visitor<Writer<JSONObject>>*)

