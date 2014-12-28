#pragma once
#include "interface/serialization/VisitorInterface.hpp"
#include <QJsonObject>
#include <QJsonArray>

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
		Visitor<Writer<JSON>>(QJsonObject obj):
			m_obj{obj}
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

