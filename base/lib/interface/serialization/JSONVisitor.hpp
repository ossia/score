#pragma once
#include "interface/serialization/VisitorInterface.hpp"
#include <QJsonObject>

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
		void visit(const T&);

		QJsonObject obj;
};

template<>
class Visitor<Writer<JSON>>
{
	public:
		template<typename T>
		void visit(T&);

		QJsonObject obj;
};