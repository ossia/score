#pragma once
#include "interface/serialization/VisitorInterface.hpp"
#include <tools/IdentifiedObject.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>

template<typename T>
T fromJsonObject(QJsonObject&& json);

class JSON
{
    public:
        static SerializationIdentifier type()
        {
            return 1;
        }
};

template<>
class Visitor<Reader<JSON>>
{
    public:
        template<typename T>
        void readFrom(const T&);


        template<typename T>
        void readFrom(const id_type<T>& obj)
        {
            m_obj["IdentifierSet"] = bool (obj.val());

            if(obj.val())
            {
                m_obj["IdentifierValue"] = *obj.val();
            }
        }


        template<typename T>
        void readFrom(const IdentifiedObject<T>& obj)
        {
            readFrom(static_cast<const NamedObject&>(obj));

            m_obj["Identifier"] = toJsonObject(obj.id());
        }

        QJsonObject m_obj;
};

template<>
class Visitor<Writer<JSON>>
{
    public:
        Visitor<Writer<JSON>>() = default;
        Visitor<Writer<JSON>> (const QJsonObject& obj) :
                               m_obj {obj}
        {}

        Visitor<Writer<JSON>> (QJsonObject&& obj) :
                               m_obj {std::move(obj) }
        {}

        template<typename T>
        void writeTo(T&);

        template<typename T>
        void writeTo(id_type<T>& obj)
        {
            bool init = m_obj["IdentifierSet"].toBool();
            int32_t val {};

            if(init)
            {
                val = m_obj["IdentifierValue"].toInt();
            }

            obj.setVal(boost::optional<int32_t> {init, val});
        }

        template<typename T>
        void writeTo(IdentifiedObject<T>& obj)
        {
            obj.setId(fromJsonObject<id_type<T>> (m_obj["Identifier"].toObject()));
        }


        QJsonObject m_obj;
};

template<typename T>
QJsonObject toJsonObject(const T& obj)
{
    Visitor<Reader<JSON>> reader;
    reader.readFrom(obj);

    return reader.m_obj;
}

template<typename T>
void fromJsonObject(QJsonObject&& json, T& obj)
{
    Visitor<Writer<JSON>> writer {json};
    writer.writeTo(obj);
}

template<typename T>
T fromJsonObject(QJsonObject&& json)
{
    T obj;
    Visitor<Writer<JSON>> writer {json};
    writer.writeTo(obj);

    return obj;
}


template<typename T>
QJsonArray toJsonArray(const QVector<T*>& array)
{
    QJsonArray arr;

    for(auto elt : array)
    {
        arr.append(toJsonObject(*elt));
    }

    return arr;
}

template<typename T>
QJsonArray toJsonArray(const std::vector<T*>& array)
{
    QJsonArray arr;

    for(auto elt : array)
    {
        arr.append(toJsonObject(*elt));
    }

    return arr;
}

template<typename T>
QJsonArray toJsonArray(const T& array)
{
    QJsonArray arr;

    for(auto elt : array)
    {
        arr.append(toJsonObject(elt));
    }

    return arr;
}



template<>
inline QJsonArray toJsonArray(const QVector<int>& array)
{
    QJsonArray arr;

    for(auto elt : array)
    {
        arr.append(elt);
    }

    return arr;
}



template<typename Value>
QJsonArray toJsonMap(const QMap<double, Value>& map)
{
    QJsonArray arr;

    for(auto key : map.keys())
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

    for(auto key : map.keys())
    {
        QJsonObject obj;
        obj["k"] = *key.val();
        obj["v"] = map[key];
        arr.append(obj);
    }

    return arr;
}


template<typename Key>
QMap<Key, double> fromJsonMap(const QJsonArray& array)
{
    QMap<Key, double> map;

    for(auto value : array)
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

    for(auto value : array)
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

    for(auto value : array)
    {
        QJsonObject obj = value.toObject();
        map[obj["k"].toDouble()] = obj["v"].toDouble();
    }

    return map;
}

inline void fromJsonArray(QJsonArray&& json_arr, QVector<int>& arr)
{
    for(const auto& elt : json_arr)
    {
        arr.push_back(elt.toInt());
    }
}

template<typename T>
void fromJsonArray(QJsonArray&& json_arr, T& arr)
{
    for(const auto& elt : json_arr)
    {
        typename T::value_type obj;
        fromJsonObject(elt.toObject(), obj);
        arr.push_back(obj);
    }
}

