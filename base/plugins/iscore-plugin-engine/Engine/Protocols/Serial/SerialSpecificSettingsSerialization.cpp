#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "SerialSpecificSettings.hpp"


template <>
void DataStreamReader::read(
    const Engine::Network::SerialSpecificSettings& n)
{
  m_stream << n.port.serialNumber() << n.text;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Engine::Network::SerialSpecificSettings& n)
{
  QString sn;
  m_stream >> sn >> n.text;

  for(const auto& port : QSerialPortInfo::availablePorts())
  {
    if(port.serialNumber() == sn)
    {
      n.port = port;
      break;
    }
  }

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::SerialSpecificSettings& n)
{
  obj["Port"] = n.port.serialNumber();
  obj["Text"] = n.text;
}


template <>
void JSONObjectWriter::write(
    Engine::Network::SerialSpecificSettings& n)
{
  auto sn = obj["Port"].toString();
  for(const auto& port : QSerialPortInfo::availablePorts())
  {
    if(port.serialNumber() == sn)
    {
      n.port = port;
      break;
    }
  }
  n.text = obj["Text"].toString();
}
