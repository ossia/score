#pragma once
#include <QDataStream>
#include "interface/serialization/VisitorInterface.hpp"
#include <tools/SettableIdentifierAlternative.hpp>

class DataStream
{
	public:
		static SerializationIdentifier type()
		{ return 2; }
};


template<>
class Visitor<Reader<DataStream>>
{
	public:
		Visitor<Reader<DataStream>>(QByteArray* array):
			m_stream{array, QIODevice::WriteOnly}
		{
			m_stream.setVersion(QDataStream::Qt_5_3);
		}

		template<typename T>
		void readFrom(const id_type<T>& obj)
		{
			m_stream << obj.val().is_initialized();
			if(obj.val().is_initialized())
				m_stream << *obj.val();
		}

		template<typename T>
		void readFrom(const T&);


		QDataStream m_stream;
};

template<>
class Visitor<Writer<DataStream>>
{
	public:
		Visitor<Writer<DataStream>>(QByteArray* array):
			m_stream{array, QIODevice::ReadOnly}
		{
			m_stream.setVersion(QDataStream::Qt_5_3);
		}

		template<typename T>
		void writeTo(id_type<T>& obj)
		{
			bool init{};
			int32_t val{};
			m_stream >> init;
			if(init)
				m_stream >> val;

			obj.setVal(boost::optional<int32_t>{init, val});
		}

		template<typename T>
		void writeTo(T&);

		QDataStream m_stream;
};

template<typename T>
QDataStream& operator<<(QDataStream& stream, const T& obj)
{
	QByteArray ar;
	Visitor<Reader<DataStream>> reader(&ar);
	reader.readFrom(obj);

	stream << ar;
	return stream;
}

template<typename T>
QDataStream& operator>>(QDataStream& stream, T& obj)
{
	QByteArray ar;
	stream >> ar;
	Visitor<Writer<DataStream>> writer(&ar);
	writer.writeTo(obj);

	return stream;
}
