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
  m_stream << n.port.serial_number << n.text << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::SerialSpecificSettings& n)
{
  QString sn;
  m_stream >> sn >> n.text >> n.rate;

  const auto& sn_str = sn.toStdString();
  for(const auto& port : serial::available_ports())
    if(port.serial_number == sn_str)
    {
      n.port = port;
      break;
    }

  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::SerialSpecificSettings& n)
{
  obj["Port"] = n.port.port_name;
  obj["PortLocation"] = n.port.system_location;
  obj["PortSN"] = n.port.serial_number;
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

  const auto& sn_str = sn.toStdString();
  const auto& loc_str = location.toStdString();
  const auto& name_str = name.toStdString();
  for(const auto& port : serial::available_ports())
  {
    if(!sn_str.empty() && port.serial_number == sn_str)
    {
      n.port = port;
      break;
    }
    if(!loc_str.empty() && port.system_location == loc_str)
    {
      n.port = port;
      break;
    }
    if(!name_str.empty() && port.port_name == name_str)
    {
      n.port = port;
      break;
    }
  }

  n.text = obj["Text"].toString();
  n.rate = obj["Rate"].toInt();
}
#endif
