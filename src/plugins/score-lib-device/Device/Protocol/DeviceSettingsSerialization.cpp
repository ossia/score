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
#include <score/serialization/OpaquePayload.hpp>

#include <QDebug>

SCORE_SERALIZE_DATASTREAM_DEFINE(Device::DeviceSettings)

namespace
{
//! The members score itself owns; everything else in the object is the
//! protocol's.
const QStringList& scoreOwnedMembers()
{
  static const QStringList members{
      QString::fromStdString(score::StringConstant().Name),
      QString::fromStdString(score::StringConstant().Protocol)};
  return members;
}

//! Read a payload the protocol wrote, in whichever format it was written:
//! a .score saved as .scorebin carries the protocol's JSON inside the blob.
QVariant makeSettings(
    const Device::ProtocolFactory& prot, const score::OpaquePayload& payload)
{
  if(payload.format == DataStream::type())
  {
    DataStream::Deserializer sub{payload.bytes};
    return prot.makeProtocolSpecificSettings(sub.toVariant());
  }

  if(payload.format == JSONObject::type())
  {
    rapidjson::Document doc;
    doc.Parse(payload.bytes.data(), payload.bytes.size());
    if(doc.HasParseError() || !doc.IsObject())
      return {};

    JSONObject::Deserializer sub{doc};
    return prot.makeProtocolSpecificSettings(sub.toVariant());
  }

  return {};
}
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(const Device::DeviceSettings& n)
{
  m_stream << n.name << n.protocol;

  // In its own blob, as readFromAbstract does for every other polymorphic
  // kind: a reader without the factory can skip it by length.
  score::OpaquePayload payload;
  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  if(auto prot = pl.get(n.protocol))
  {
    QByteArray bytes;
    {
      DataStream::Serializer sub{&bytes};
      prot->serializeProtocolSpecificSettings(
          n.deviceSpecificSettings, sub.toVariant());
    }
    payload = score::OpaquePayload{DataStream::type(), std::move(bytes)};
  }
  else
  {
    // Nothing here understands them, so pass on exactly what we were given.
    payload = score::OpaquePayload::fromBlob(n.opaqueSettings);
  }

  m_stream << payload.toBlob();
  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::DeviceSettings& n)
{
  m_stream >> n.name >> n.protocol;

  QByteArray blob;
  m_stream >> blob;

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  if(auto prot = pl.get(n.protocol))
    n.deviceSpecificSettings = makeSettings(*prot, score::OpaquePayload::fromBlob(blob));
  else
    n.opaqueSettings = std::move(blob);

  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONReader::read(const Device::DeviceSettings& n)
{
  stream.StartObject();
  obj[strings.Name] = n.name;
  obj[strings.Protocol] = n.protocol;

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  if(auto prot = pl.get(n.protocol))
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
  else
    score::OpaquePayload::fromBlob(n.opaqueSettings).write(this->toVariant());

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

  n.opaqueSettings = score::OpaquePayload::fromJson(base, scoreOwnedMembers()).toBlob();
}
