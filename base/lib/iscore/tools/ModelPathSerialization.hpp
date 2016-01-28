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
