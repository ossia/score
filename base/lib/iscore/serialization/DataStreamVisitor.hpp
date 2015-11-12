#pragma once
#include <QDataStream>
#include <type_traits>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

/**
 * This file contains facilities
 * to serialize an object using QDataStream.
 *
 * Generally, it is used with QByteArrays, but it works with any QIODevice.
 */


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
class Visitor<Reader<DataStream>> final : public AbstractVisitor
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
        static auto marshall(const T& t)
        {
            QByteArray arr;
            Visitor<Reader<DataStream>> reader{&arr};
            reader.readFrom(t);
            return arr;
        }

        template<typename T>
        void readFrom(const Id<T>& obj)
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

        template<typename T, std::enable_if_t<!std::is_enum<T>::value>* = nullptr>
        void readFrom(const T&);

        template<typename T>
        void readFrom(const TreeNode<T>&);
        template<typename T>
        void readFrom(const TreePath<T>&);

        template<typename... Args>
        void readFrom(const eggs::variants::variant<Args...>&);

        template<typename T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
        void readFrom(const T& elt)
        {
            m_stream << (int32_t) elt;
        }

        /**
         * @brief insertDelimiter
         *
         * Adds a delimiter that is to be checked by the reader.
         */
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
        static auto unmarshall(const QByteArray& arr)
        {
            T data;
            Visitor<Writer<DataStream>> wrt{arr};
            wrt.writeTo(data);
            return data;
        }

        template<typename T>
        void writeTo(Id<T>& obj)
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
            Id<T> id;
            writeTo(id);
            obj.setId(std::move(id));
        }

        template<typename T, std::enable_if_t<!std::is_enum<T>::value>* = nullptr>
        void writeTo(T&);

        template<typename T>
        void writeTo(TreeNode<T>&);
        template<typename T>
        void writeTo(TreePath<T>&);

        template<typename... Args>
        void writeTo(eggs::variants::variant<Args...>&);

        template<typename T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
        void writeTo(T& elt)
        {
            int32_t e;
            m_stream >> e;
            elt = static_cast<T>(e);
        }

        /**
         * @brief checkDelimiter
         *
         * Checks if a delimiter is present at the current
         * stream position, and fails if it isn't.
         */
        void checkDelimiter()
        {
            int val{};
            m_stream >> val;

            if(val != int32_t (0xDEADBEEF))
            {
                ISCORE_BREAKPOINT;
                throw std::runtime_error("Corrupt save file.");
            }
        }

        QDataStream m_stream;
};


// TODO instead why not add a iscore_serializable tag to our classes ?
template<typename T,
         std::enable_if_t<
             ! std::is_arithmetic<T>::value
          && ! std::is_same<T, QStringList>::value>* = nullptr>
QDataStream& operator<< (QDataStream& stream, const T& obj)
{
    Visitor<Reader<DataStream>> reader(stream.device());
    reader.readFrom(obj);
    return stream;
}

template<typename T,
         std::enable_if_t<
             ! std::is_arithmetic<T>::value
          && ! std::is_same<T, QStringList>::value>* = nullptr>
QDataStream& operator>> (QDataStream& stream, T& obj)
{
    Visitor<Writer<DataStream>> writer(stream.device());
    writer.writeTo(obj);

    return stream;
}


Q_DECLARE_METATYPE(Visitor<Reader<DataStream>>*)
Q_DECLARE_METATYPE(Visitor<Writer<DataStream>>*)
