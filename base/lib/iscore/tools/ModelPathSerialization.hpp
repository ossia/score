#pragma once
#include <iscore/tools/ModelPath.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

template<typename Object>
QDataStream& operator<< (QDataStream& stream, const Path<Object>& obj)
{
    Visitor<Reader<DataStream>> reader(stream.device());
    reader.readFrom(obj.unsafePath());
    return stream;
}

template<typename Object>
QDataStream& operator>> (QDataStream& stream, Path<Object>& obj)
{
    Visitor<Writer<DataStream>> writer(stream.device());
    writer.writeTo(obj.unsafePath_ref());

    return stream;
}

template<typename T>
struct TSerializer<DataStream, void, Path<T>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const Path<T>& path)
        {
            s.readFrom(path.unsafePath());
        }

        static void writeTo(
                DataStream::Deserializer& s,
                Path<T>& path)
        {
            s.writeTo(path.unsafePath_ref());
        }
};
