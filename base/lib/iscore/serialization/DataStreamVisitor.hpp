#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <QByteArray>
#include <QDataStream>
#include <sys/types.h>
#include <stdexcept>
#include <type_traits>

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
namespace iscore
{
struct ApplicationContext;
}
class QIODevice;
class QStringList;
template <typename model> class IdentifiedObject;

class DataStreamInput
{
        QDataStream& m_stream;
    public:
        auto& stream() const
        { return m_stream; }

        DataStreamInput(QDataStream& s):
            m_stream{s}
        {

        }

        template<typename T>
        friend DataStreamInput& operator<<(DataStreamInput& s, T&& obj)
        {
            s.m_stream << obj;
            return s;
        }
};

class DataStreamOutput
{
        QDataStream& m_stream;
    public:
        auto& stream() const
        { return m_stream; }

        DataStreamOutput(QDataStream& s):
            m_stream{s}
        {

        }

        template<typename T>
        friend DataStreamOutput& operator>>(DataStreamOutput& s, T&& obj)
        {
            s.m_stream >> obj;
            return s;
        }
};


/**
 * This file contains facilities
 * to serialize an object using QDataStream.
 *
 * Generally, it is used with QByteArrays, but it works with any QIODevice.
 */
class DataStream;

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
class ISCORE_LIB_BASE_EXPORT Visitor<Reader<DataStream>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, DataStream::type()}; }

        Visitor();
        Visitor(const Visitor&) = delete;
        Visitor& operator=(const Visitor&) = delete;

        Visitor(QByteArray* array);
        Visitor(QIODevice* dev);

        template<typename T>
        static auto marshall(const T& t)
        {
            QByteArray arr;
            Visitor<Reader<DataStream>> reader{&arr};
            reader.readFrom(t);
            return arr;
        }

        template<template<class...> class T,
                 typename... Args>
        void readFrom(
                const T<Args...>& obj,
                typename std::enable_if_t<
                    is_template<T<Args...>>::value &&
                    !is_abstract_base<T<Args...>>::value> * = 0)
        {
            TSerializer<DataStream, T<Args...>>::readFrom(*this, obj);
        }

        template<typename T,
                 std::enable_if_t<
                     is_abstract_base<T>::value
                     >* = nullptr>
        void readFrom(const T& obj)
        {
            AbstractSerializer<DataStream, T>::readFrom(*this, obj);
        }

        template<typename T>
        void readFrom_impl(const T&);

        template<typename T,
                 std::enable_if_t<
                     !std::is_enum<T>::value &&
                     !is_template<T>::value &&
                     !is_abstract_base<T>::value
                     >* = nullptr>
        void readFrom(const T&);

        template<typename T,
                 std::enable_if_t<
                     std::is_enum<T>::value &&
                     !is_template<T>::value>* = nullptr>
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

        auto& stream() { return m_stream; }
    private:
        QDataStream m_stream_impl;

    public:
        const iscore::ApplicationContext& context;
        DataStreamInput m_stream{m_stream_impl};
};

template<>
class ISCORE_LIB_BASE_EXPORT Visitor<Writer<DataStream>> : public AbstractVisitor
{
    public:
        using is_visitor_tag = std::integral_constant<bool, true>;

        VisitorVariant toVariant() { return {*this, DataStream::type()}; }

        Visitor();
        Visitor(const Visitor&) = delete;
        Visitor& operator=(const Visitor&) = delete;

        Visitor(const QByteArray& array);
        Visitor(QIODevice* dev);


        template<typename T>
        static auto unmarshall(const QByteArray& arr)
        {
            T data;
            Visitor<Writer<DataStream>> wrt{arr};
            wrt.writeTo(data);
            return data;
        }

        template<
                template<class...> class T,
                typename... Args>
        void writeTo(
                T<Args...>& obj,
                typename std::enable_if<is_template<T<Args...>>::value, void>::type * = 0)
        {
            TSerializer<DataStream, T<Args...>>::writeTo(*this, obj);
        }

        template<typename T,
                 std::enable_if_t<!std::is_enum<T>::value && !is_template<T>::value>* = nullptr >
        void writeTo(T&);

        template<typename T,
                 std::enable_if_t<std::is_enum<T>::value && !is_template<T>::value>* = nullptr>
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

        auto& stream() { return m_stream; }
    private:
        QDataStream m_stream_impl;

    public:
        const iscore::ApplicationContext& context;
        DataStreamOutput m_stream{m_stream_impl};
};


// TODO instead why not add a iscore_serializable tag to our classes ?
template<typename T,
         std::enable_if_t<
             ! std::is_arithmetic<T>::value
          && ! std::is_same<T, QStringList>::value>* = nullptr>
QDataStream& operator<< (QDataStream& stream, const T& obj)
{
    Visitor<Reader<DataStream>> reader{stream.device()};
    reader.readFrom(obj);
    return stream;
}

template<typename T,
         std::enable_if_t<
             ! std::is_arithmetic<T>::value
          && ! std::is_same<T, QStringList>::value>* = nullptr>
QDataStream& operator>> (QDataStream& stream, T& obj)
{
    Visitor<Writer<DataStream>> writer{stream.device()};
    writer.writeTo(obj);

    return stream;
}

template<typename U>
struct TSerializer<DataStream, Id<U>>
{
    static void readFrom(
            DataStream::Serializer& s,
            const Id<U>& obj)
    {
        s.stream() << bool (obj.val());

        if(obj.val())
        {
            s.stream() << *obj.val();
        }
    }

    static void writeTo(
            DataStream::Deserializer& s,
            Id<U>& obj)
    {
        bool init {};
        int32_t val {};
        s.stream() >> init;

        if(init)
        {
            s.stream() >> val;
            obj.setVal(val);
        }
        else
        {
            obj.unset();
        }
    }
};


template<typename T>
struct TSerializer<DataStream, IdentifiedObject<T>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const IdentifiedObject<T>& obj)
        {
            s.readFrom(static_cast<const NamedObject&>(obj));
            s.readFrom(obj.id());
        }

        static void writeTo(
                DataStream::Deserializer& s,
                IdentifiedObject<T>& obj)
        {
            Id<T> id;
            s.writeTo(id);
            obj.setId(std::move(id));
        }

};
#include <boost/optional.hpp>
template<typename T>
struct TSerializer<DataStream, boost::optional<T>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const boost::optional<T>& obj)
        {
            s.stream() << static_cast<bool>(obj);

            if(obj)
            {
                s.stream() << get(obj);
            }
        }

        static void writeTo(
                DataStream::Deserializer& s,
                boost::optional<T>& obj)
        {
            bool b {};
            s.stream() >> b;

            if(b)
            {
                T val;
                s.stream() >> val;
                obj = val;
            }
            else
            {
                obj.reset();
            }
        }
};

template<typename T>
struct TSerializer<DataStream, QList<T>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const QList<T>& obj)
        {
            s.stream() << obj;
        }

        static void writeTo(
                DataStream::Deserializer& s,
                QList<T>& obj)
        {
            s.stream() >> obj;
        }
};

Q_DECLARE_METATYPE(Visitor<Reader<DataStream>>*)
Q_DECLARE_METATYPE(Visitor<Writer<DataStream>>*)
