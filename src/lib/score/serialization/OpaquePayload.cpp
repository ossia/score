#include <score/serialization/OpaquePayload.hpp>

#include <QIODevice>

namespace score
{
namespace
{
// Which format is inside, since a JSON payload can end up in a binary blob:
// autosave and interval moves serialise to binary whatever the document is.
constexpr auto foreign_json_marker = "\0score-opaque-json";
constexpr int foreign_json_marker_size = 18;
constexpr auto foreign_binary_key = "$score-opaque-binary";

QByteArray membersExcept(const rapidjson::Value& base, const QStringList& owned)
{
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
}

OpaquePayload
OpaquePayload::fromJson(const rapidjson::Value& base, const QStringList& owned) noexcept
{
  if(!base.IsObject())
    return {};

  // Written by a previous pass that had binary data to keep.
  if(auto it = base.FindMember(foreign_binary_key);
     it != base.MemberEnd() && it->value.IsString())
  {
    return OpaquePayload{
        DataStream::type(),
        QByteArray::fromBase64(QByteArray{
            it->value.GetString(), (int)it->value.GetStringLength()})};
  }

  auto members = membersExcept(base, owned);
  if(members.isEmpty())
    return {};
  return OpaquePayload{JSONObject::type(), std::move(members)};
}

OpaquePayload OpaquePayload::fromDataStream(DataStream::Deserializer& vis) noexcept
{
  auto* dev = vis.m_stream.stream.device();
  if(!dev)
    return {};

  auto tail = dev->readAll();
  if(tail.isEmpty())
    return {};

  if(tail.startsWith(QByteArray::fromRawData(
         foreign_json_marker, foreign_json_marker_size)))
  {
    return OpaquePayload{
        JSONObject::type(), tail.mid(foreign_json_marker_size)};
  }

  return OpaquePayload{DataStream::type(), std::move(tail)};
}

QByteArray OpaquePayload::toBlob() const noexcept
{
  if(empty())
    return {};

  QByteArray out;
  QDataStream s{&out, QIODevice::WriteOnly};
  s << (int32_t)format << bytes;
  return out;
}

OpaquePayload OpaquePayload::fromBlob(const QByteArray& blob) noexcept
{
  if(blob.isEmpty())
    return {};

  QDataStream s{blob};
  int32_t fmt{};
  QByteArray b;
  s >> fmt >> b;

  if(s.status() != QDataStream::Ok)
    return {};
  if(fmt != DataStream::type() && fmt != JSONObject::type())
    return {};

  return OpaquePayload{fmt, std::move(b)};
}

void OpaquePayload::write(const VisitorVariant& vis) const noexcept
{
  if(empty())
    return;

  if(vis.identifier == DataStream::type())
  {
    auto& s = static_cast<DataStream::Serializer&>(vis.visitor);
    if(format == JSONObject::type())
      s.m_stream.stream.writeRawData(foreign_json_marker, foreign_json_marker_size);
    s.m_stream.stream.writeRawData(bytes.constData(), bytes.size());
  }
  else if(vis.identifier == JSONObject::type())
  {
    auto& s = static_cast<JSONObject::Serializer&>(vis.visitor);

    if(format == DataStream::type())
    {
      const auto encoded = bytes.toBase64();
      s.stream.Key(foreign_binary_key);
      s.stream.String(encoded.constData(), encoded.size());
      return;
    }

    rapidjson::Document d;
    d.Parse(bytes.data(), bytes.size());
    if(d.HasParseError() || !d.IsObject())
      return;

    for(const auto& m : d.GetObject())
    {
      s.stream.Key(m.name.GetString(), m.name.GetStringLength());
      m.value.Accept(s.stream);
    }
  }
}
}
