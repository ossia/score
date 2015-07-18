#pragma once
#include <QDataStream>
#include <type_traits>
#include "iscore/serialization/VisitorInterface.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class DataStream;
template<> class Visitor<Reader<DataStream>>;
template<> class Visitor<Writer<DataStream>>;

class DataStream
{
    public:
        using Serializer = Visitor<Reader<DataStream>>;
        using Deserializer = Visitor<Writer<DataStream>>;
        static constexpr SerializationIdentifier type()
        {
            return 2;
        }
};

template<>
class Visitor<Reader<DataStream>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, DataStream::type()}; }

        Visitor<Reader<DataStream>>() = default;
        Visitor<Reader<DataStream>>(const Visitor<Reader<DataStream>>&) = delete;
        Visitor<Reader<DataStream>>& operator=(const Visitor<Reader<DataStream>>&) = delete;

        Visitor<Reader<DataStream>> (QByteArray* array) :
                                     m_stream {array, QIODevice::WriteOnly}
        {
            m_stream.setVersion(QDataStream::Qt_5_3);
        }

        Visitor<Reader<DataStream>> (QIODevice* dev) :
                                     m_stream {dev}
        {
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
class Visitor<Writer<DataStream>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, DataStream::type()}; }

        Visitor<Writer<DataStream>>() = default;
        Visitor<Writer<DataStream>>(const Visitor<Writer<DataStream>>&) = delete;
        Visitor<Writer<DataStream>>& operator=(const Visitor<Writer<DataStream>>&) = delete;

        Visitor<Writer<DataStream>> (const QByteArray& array) :
                                     m_stream {array}
        {
            m_stream.setVersion(QDataStream::Qt_5_3);
        }


        Visitor<Writer<DataStream>> (QIODevice* dev) :
                                     m_stream {dev}
        {
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
            int val{};
            m_stream >> val;

            if(val != int32_t (0xDEADBEEF))
            {
                throw std::runtime_error("Corrupt save file.");
            }
        }

        QDataStream m_stream;
};

// TODO instead why not add a iscore_serializable tag to our classes ?
template<typename T,
         typename = std::enable_if_t<
             not std::is_arithmetic<T>::value
         and not std::is_same<T, QStringList>::value>>
QDataStream& operator<< (QDataStream& stream, const T& obj)
{
    Visitor<Reader<DataStream>> reader(stream.device());
    reader.readFrom(obj);
    return stream;
}

template<typename T,
         typename = std::enable_if_t<
             not std::is_arithmetic<T>::value
         and not std::is_same<T, QStringList>::value>>
QDataStream& operator>> (QDataStream& stream, T& obj)
{
    Visitor<Writer<DataStream>> writer(stream.device());
    writer.writeTo(obj);

    return stream;
}



Q_DECLARE_METATYPE(Visitor<Reader<DataStream>>*)
Q_DECLARE_METATYPE(Visitor<Writer<DataStream>>*)
