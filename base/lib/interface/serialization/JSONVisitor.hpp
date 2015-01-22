#pragma once
#include "interface/serialization/VisitorInterface.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>

class JSON
{
	public:
		static SerializationIdentifier type()
		{ return 1; }
};

template<>
class Visitor<Reader<JSON>>
{
	public:
		template<typename T>
		void readFrom(const T&);

		QJsonObject m_obj;
};

template<>
class Visitor<Writer<JSON>>
{
	public:
		Visitor<Writer<JSON>>() = default;
		Visitor<Writer<JSON>>(const QJsonObject& obj):
			m_obj{obj}
		{}

		Visitor<Writer<JSON>>(QJsonObject&& obj):
			m_obj{std::move(obj)}
		{}
		template<typename T>
		void writeTo(T&);

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
	Visitor<Writer<JSON>> writer{json};
	writer.writeTo(obj);
}

inline QJsonArray toJsonArray(const QVector<int>& array)
{
	QJsonArray arr;
	for(auto elt : array)
	{
		arr.append(elt);
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
		map[Key{obj["k"].toInt()}] = obj["v"].toDouble();
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

