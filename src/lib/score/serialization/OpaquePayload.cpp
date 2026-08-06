#include <score/serialization/OpaquePayload.hpp>

#include <QIODevice>

namespace score
{
QByteArray
capturedMembers(const rapidjson::Value& base, const QStringList& owned) noexcept
{
  if(!base.IsObject())
    return {};

  rapidjson::StringBuffer buf;
  JsonWriter w{buf};
  w.StartObject();
  for(const auto& m : base.GetObject())
  {
    const auto name = QString::fromUtf8(m.name.GetString(), m.name.GetStringLength());
    if(owned.contains(name))
      continue;
    w.Key(m.name.GetString(), m.name.GetStringLength());
    m.value.Accept(w);
  }
  w.EndObject();

  // "{}" -- there was nothing but what we already own.
  if(buf.GetLength() <= 2)
    return {};
  return QByteArray{buf.GetString(), (int)buf.GetLength()};
}

void writeCapturedMembers(JsonWriter& stream, const QByteArray& captured) noexcept
{
  if(captured.isEmpty())
    return;

  rapidjson::Document d;
  d.Parse(captured.data(), captured.size());
  if(d.HasParseError() || !d.IsObject())
    return;

  for(const auto& m : d.GetObject())
  {
    stream.Key(m.name.GetString(), m.name.GetStringLength());
    m.value.Accept(stream);
  }
}

QByteArray capturedTail(DataStream::Deserializer& vis) noexcept
{
  if(auto* dev = vis.m_stream.stream.device())
    return dev->readAll();
  return {};
}

void writeCapturedTail(DataStream::Serializer& s, const QByteArray& captured) noexcept
{
  if(!captured.isEmpty())
    s.m_stream.stream.writeRawData(captured.constData(), captured.size());
}
}
