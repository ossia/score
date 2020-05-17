// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include "SerialSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QString>

template <>
void DataStreamReader::read(const Protocols::SerialSpecificSettings& n)
{
  m_stream << n.port.serialNumber() << n.text;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SerialSpecificSettings& n)
{
  QString sn;
  m_stream >> sn >> n.text;

  for (const auto& port : QSerialPortInfo::availablePorts())
  {
    if (port.serialNumber() == sn)
    {
      n.port = port;
      break;
    }
  }

  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SerialSpecificSettings& n)
{
  obj["Port"] = n.port.serialNumber();
  obj["Text"] = n.text;
}

template <>
void JSONWriter::write(Protocols::SerialSpecificSettings& n)
{
  auto sn = obj["Port"].toString();
  for (const auto& port : QSerialPortInfo::availablePorts())
  {
    if (port.serialNumber() == sn)
    {
      n.port = port;
      break;
    }
  }
  n.text = obj["Text"].toString();
}
#endif
