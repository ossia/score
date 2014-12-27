#pragma once
#include <QDataStream>
#include "interface/serialization/VisitorInterface.hpp"
class DataStreamReader {};
class DataStreamWriter {};

template<>
class Visitor<DataStreamReader>
{
	public:
		Visitor<DataStreamReader>(QByteArray* array):
			m_stream{array, QIODevice::ReadOnly}
		{
			m_stream.setVersion(QDataStream::Qt_5_3);
		}

		template<typename T>
		void visit(T&);

	private:
		QDataStream m_stream;
};

template<>
class Visitor<DataStreamWriter>
{
	public:
		Visitor<DataStreamWriter>(QByteArray* array):
			m_stream{array, QIODevice::WriteOnly}
		{
			m_stream.setVersion(QDataStream::Qt_5_3);
		}

		template<typename T>
		void visit(T&);

	private:
		QDataStream m_stream;
};