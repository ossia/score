// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSONVisitor.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/application/ApplicationComponents.hpp>

rapidjson::MemoryPoolAllocator<>& JsonPool() noexcept
{
  static rapidjson::MemoryPoolAllocator<> alloc;
  return alloc;
}

rapidjson::Document clone(const rapidjson::Value& val) noexcept
{
  rapidjson::Document v;
  v.CopyFrom(val, JsonPool(), true);
  return v;
}

rapidjson::Document toValue(const JSONReader& r) noexcept
{
  rapidjson::Document doc;
  doc.Parse(r.buffer.GetString(), r.buffer.GetLength());
  return doc;
}

JSONReader::JSONReader()
    : obj{*this}
    , components{score::AppComponents()}
    , strings{score::StringConstant()}
{
}

JSONWriter::JSONWriter(const rapidjson::Value& o)
    : base{o}
    , components{score::AppComponents()}
    , strings{score::StringConstant()}
{
}
JSONWriter::JSONWriter(const JsonValue& o)
  : base{o.obj}
  , components{score::AppComponents()}
  , strings{score::StringConstant()}
{
}

void TSerializer<DataStream, rapidjson::Document>::readFrom(DataStream::Serializer& s, const rapidjson::Document& obj)
{
  s.stream() << jsonToByteArray(obj);
}

void TSerializer<DataStream, rapidjson::Document>::writeTo(DataStream::Deserializer& s, rapidjson::Document& obj)
{
  QByteArray arr;
  s.stream() >> arr;
  obj.Parse(arr.data(), arr.size());
}
