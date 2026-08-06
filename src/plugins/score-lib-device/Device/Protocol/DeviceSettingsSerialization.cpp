// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceSettings.hpp"
#include "ProtocolFactoryInterface.hpp"

#include <Device/Protocol/ProtocolList.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QDebug>

#include <stdexcept>

SCORE_SERALIZE_DATASTREAM_DEFINE(Device::DeviceSettings)

namespace
{
[[noreturn]] void throwMissingProtocol(const Device::DeviceSettings& n)
{
  // The binary format writes protocol settings inline with no length prefix, so
  // a reader without the factory cannot skip them: it lands mid-payload on the
  // trailing delimiter and reports the whole file as corrupt (and SIGTRAPs on
  // the way, since checkDelimiter breakpoints before it throws). Say what is
  // actually wrong instead.
  const QString msg
      = QStringLiteral(
            "device '%1' uses protocol %2, which this build does not have. The "
            "binary format cannot preserve settings for an unknown protocol: use "
            "the JSON .score format to move this document between machines.")
            .arg(n.name)
            .arg(QString::fromUtf8(score::uuids::toByteArray(n.protocol.impl())));
  throw std::runtime_error{msg.toStdString()};
}

bool isReservedMember(const rapidjson::Value::Member& m) noexcept
{
  const auto& s = score::StringConstant();
  const std::string_view name{m.name.GetString(), m.name.GetStringLength()};
  return name == s.Name || name == s.Protocol;
}

//! Everything in the serialized device except the two members score itself
//! owns, i.e. exactly what the protocol factory would have written.
QByteArray captureProtocolMembers(const rapidjson::Value& base)
{
  if(!base.IsObject())
    return {};

  rapidjson::StringBuffer buf;
  JsonWriter w{buf};
  w.StartObject();
  for(const auto& m : base.GetObject())
  {
    if(isReservedMember(m))
      continue;
    w.Key(m.name.GetString(), m.name.GetStringLength());
    m.value.Accept(w);
  }
  w.EndObject();

  // An object with no protocol-specific members is not worth carrying around.
  if(buf.GetLength() <= 2)
    return {};
  return QByteArray{buf.GetString(), (int)buf.GetLength()};
}

void writeProtocolMembers(JsonWriter& stream, const QByteArray& blob)
{
  rapidjson::Document d;
  d.Parse(blob.data(), blob.size());
  if(d.HasParseError() || !d.IsObject())
    return;

  for(const auto& m : d.GetObject())
  {
    stream.Key(m.name.GetString(), m.name.GetStringLength());
    m.value.Accept(stream);
  }
}
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(const Device::DeviceSettings& n)
{
  m_stream << n.name << n.protocol;

  // TODO try to see if this pattern is refactorable with the similar thing
  // usef for CurveSegmentData.

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  auto prot = pl.get(n.protocol);
  if(prot)
  {
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
  }
  else if(!n.opaqueSettings.isEmpty())
  {
    // Deliberately not fatal: this path also runs when a document is opened,
    // not only when the user asks to save, so throwing would make documents
    // naming an unknown protocol impossible to open at all. The settings stay
    // in `opaqueSettings` and survive a JSON save; only the binary format
    // cannot carry them.
    qDebug() << "Warning: settings of device" << n.name << "use protocol"
             << score::uuids::toByteArray(n.protocol.impl())
             << "which is not available; they cannot be written to the binary "
                "format. Save as .score to preserve them.";
  }

  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::DeviceSettings& n)
{
  m_stream >> n.name >> n.protocol;

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  if(auto prot = pl.get(n.protocol))
  {
    n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
    checkDelimiter();
    return;
  }

  // No factory here. If the writer had none either it wrote no payload, so the
  // delimiter comes next and the round-trip is consistent -- that case has to
  // keep working. Otherwise the payload was written by a build that did have
  // the protocol, and since it carries no length prefix there is no way to skip
  // it: the read cannot continue.
  int32_t next{};
  m_stream.stream >> next;
  if(next != int32_t(0xDEADBEEF))
    throwMissingProtocol(n);
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONReader::read(const Device::DeviceSettings& n)
{
  stream.StartObject();
  obj[strings.Name] = n.name;
  obj[strings.Protocol] = n.protocol;

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  auto prot = pl.get(n.protocol);
  if(prot)
  {
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
  }
  else
  {
    writeProtocolMembers(stream, n.opaqueSettings);
  }
  stream.EndObject();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONWriter::write(Device::DeviceSettings& n)
{
  n.name = obj[strings.Name].toString();
  n.protocol <<= obj[strings.Protocol];

  auto pl = components.findInterfaces<Device::ProtocolFactoryList>();
  if(pl)
  {
    if(auto prot = pl->get(n.protocol))
    {
      n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
      return;
    }
  }

  n.opaqueSettings = captureProtocolMembers(base);
}
