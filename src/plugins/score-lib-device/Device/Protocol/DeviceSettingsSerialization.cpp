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

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamReader::read(const Device::DeviceSettings& n)
{
  m_stream << n.name << n.protocol;

  // TODO try to see if this pattern is refactorable with the similar thing
  // usef for CurveSegmentData.

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  auto prot = pl.get(n.protocol);
  if (prot)
  {
    prot->serializeProtocolSpecificSettings(
        n.deviceSpecificSettings, this->toVariant());
  }
  else
  {
    qDebug() << "Warning: could not serialize device " << n.name;
  }

  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::DeviceSettings& n)
{
  m_stream >> n.name >> n.protocol;

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  auto prot = pl.get(n.protocol);
  if (prot)
  {
    n.deviceSpecificSettings
        = prot->makeProtocolSpecificSettings(this->toVariant());
  }
  else
  {
    qDebug() << "Warning: could not load device " << n.name;
  }

  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONReader::read(const Device::DeviceSettings& n)
{
  stream.StartObject();
  obj[strings.Name] = n.name;
  obj[strings.Protocol] = n.protocol;

  auto& pl = components.interfaces<Device::ProtocolFactoryList>();
  auto prot = pl.get(n.protocol);
  if (prot)
  {
    prot->serializeProtocolSpecificSettings(
        n.deviceSpecificSettings, this->toVariant());
  }
  else
  {
    qDebug() << "Warning: could not serialize device " << n.name;
  }
  stream.EndObject();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONWriter::write(Device::DeviceSettings& n)
{
  n.name = obj[strings.Name].toString();
  n.protocol <<= obj[strings.Protocol];

  auto pl = components.findInterfaces<Device::ProtocolFactoryList>();
  if (pl)
  {
    if (auto prot = pl->get(n.protocol))
    {
      n.deviceSpecificSettings
          = prot->makeProtocolSpecificSettings(this->toVariant());
    }
    else
    {
      qDebug() << "Warning: could not load device " << n.name;
    }
  }
}
