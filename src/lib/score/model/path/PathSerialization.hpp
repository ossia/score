#pragma once
#include <score/model/path/Path.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <typename Object>
QDataStream& operator<<(QDataStream& stream, const Path<Object>& obj)
{
  DataStreamReader reader(stream.device());
  reader.readFrom(obj.unsafePath());
  return stream;
}

template <typename Object>
QDataStream& operator>>(QDataStream& stream, Path<Object>& obj)
{
  DataStreamWriter writer(stream.device());
  writer.writeTo(obj.unsafePath());

  return stream;
}

template <typename T>
struct TSerializer<DataStream, Path<T>>
{
  static void readFrom(DataStream::Serializer& s, const Path<T>& path)
  {
    s.readFrom(path.unsafePath());
  }

  static void writeTo(DataStream::Deserializer& s, Path<T>& path)
  {
    s.writeTo(path.unsafePath());
  }
};

template <typename T>
struct TSerializer<JSONObject, Path<T>>
{
  static void readFrom(JSONObject::Serializer& s, const Path<T>& path)
  {
    s.readFrom(path.unsafePath());
  }

  static void writeTo(JSONObject::Deserializer& s, Path<T>& path)
  {
    s.writeTo(path.unsafePath());
  }
};
