#pragma once
#include "interface/serialization/VisitorInterface.hpp"
#include <QJsonObject>

class JSONReader {};
template<>
class Visitor<JSONReader>
{
	public:
		template<typename T>
		void visit(T&);

		QJsonObject obj;
};

class JSONWriter {};
template<>
class Visitor<JSONWriter>
{
	public:
		template<typename T>
		void visit(T&);

		QJsonObject obj;
};