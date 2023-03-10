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
  m_stream << n.port.serialNumber() << n.text << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SerialSpecificSettings& n)
{
  QString sn;
  m_stream >> sn >> n.text >> n.rate;

  for(const auto& port : QSerialPortInfo::availablePorts())
    if(port.serialNumber() == sn)
    {
      n.port = port;
      break;
    }

  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SerialSpecificSettings& n)
{
  obj["Port"] = n.port.portName();
  obj["PortLocation"] = n.port.systemLocation();
  obj["PortSN"] = n.port.systemLocation();
  obj["Text"] = n.text;
  obj["Rate"] = n.rate;
}

template <>
void JSONWriter::write(Protocols::SerialSpecificSettings& n)
{
  QString name, sn, location;

  name = obj["Port"].toString();
  if(auto opt_sn = obj.tryGet("PortSN"))
    sn = opt_sn->toString();
  if(auto opt_loc = obj.tryGet("PortLocation"))
    location = opt_loc->toString();

  for(const auto& port : QSerialPortInfo::availablePorts())
  {
    if(!sn.isEmpty() && port.serialNumber() == sn)
    {
      n.port = port;
      break;
    }
    if(!location.isEmpty() && port.systemLocation() == location)
    {
      n.port = port;
      break;
    }
    if(!name.isEmpty() && port.portName() == name)
    {
      n.port = port;
      break;
    }
  }

  n.text = obj["Text"].toString();
  n.rate = obj["Rate"].toInt();
}
#endif
