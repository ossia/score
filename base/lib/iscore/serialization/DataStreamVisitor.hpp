#pragma once
#include <QDataStream>
#include "iscore/serialization/VisitorInterface.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class DataStream
{
    public:
        static SerializationIdentifier type()
        {
            return 2;
        }
};


template<>
class Visitor<Reader<DataStream>>
{
    public:
        Visitor<Reader<DataStream>> (QByteArray* array) :
                                     m_stream {array, QIODevice::WriteOnly}
        {
            m_stream.setVersion(QDataStream::Qt_5_3);
        }

        template<typename T>
        void readFrom(const id_type<T>& obj)
        {
            m_stream << bool (obj.val());

            if(obj.val())
            {
                m_stream << *obj.val();
            }
        }

        template<typename T>
        void readFrom(const IdentifiedObject<T>& obj)
        {
            readFrom(static_cast<const NamedObject&>(obj));
            readFrom(obj.id());
        }

        template<typename T>
        void readFrom(const T&);

        void insertDelimiter()
        {
            m_stream << int32_t (0xDEADBEEF);
        }

        QDataStream m_stream;
};

template<>
class Visitor<Writer<DataStream>>
{
    public:
        // TODO a const ref is sufficient here
        Visitor<Writer<DataStream>> (QByteArray* array) :
                                     m_stream {array, QIODevice::ReadOnly}
        {
            m_stream.setVersion(QDataStream::Qt_5_3);
        }

        template<typename T>
        void writeTo(id_type<T>& obj)
        {
            bool init {};
            int32_t val {};
            m_stream >> init;

            if(init)
            {
                m_stream >> val;
                obj.setVal(val);
            }
            else
            {
                obj.unset();
            }
        }

        template<typename T>
        void writeTo(IdentifiedObject<T>& obj)
        {
            id_type<T> id;
            writeTo(id);
            obj.setId(std::move(id));
        }

        template<typename T>
        void writeTo(T&);

        void checkDelimiter()
        {
            int val {};
            m_stream >> val;

            if(val != int32_t (0xDEADBEEF))
            {
                throw std::runtime_error("Corrupt QDataStream");
            }
        }



        QDataStream m_stream;
};

template<typename T>
QDataStream& operator<< (QDataStream& stream, const T& obj)
{
    QByteArray ar;
    Visitor<Reader<DataStream>> reader(&ar);
    reader.readFrom(obj);

    stream << ar;
    return stream;
}

template<typename T>
QDataStream& operator>> (QDataStream& stream, T& obj)
{
    QByteArray ar;
    stream >> ar;
    Visitor<Writer<DataStream>> writer(&ar);
    writer.writeTo(obj);

    return stream;
}
