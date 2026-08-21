#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::CANSpecificSettings& n)
{
  m_stream << n.interfaceName << n.dbcPath << n.nodeIdOffset << n.float32Override << n.fd
           << n.filterToDatabase;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::CANSpecificSettings& n)
{
  m_stream >> n.interfaceName >> n.dbcPath >> n.nodeIdOffset >> n.float32Override >> n.fd
      >> n.filterToDatabase;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::CANSpecificSettings& n)
{
  obj["Interface"] = n.interfaceName;
  obj["DBC"] = n.dbcPath;
  obj["NodeIdOffset"] = n.nodeIdOffset;
  obj["Float32Override"] = n.float32Override;
  obj["FD"] = n.fd;
  obj["FilterToDatabase"] = n.filterToDatabase;
}

template <>
void JSONWriter::write(Protocols::CANSpecificSettings& n)
{
  // tryGet throughout: a document saved before any one of these settings
  // existed must still load, keeping the field's default.
  if(auto v = obj.tryGet("Interface"))
    n.interfaceName = QString::fromStdString(v->toStdString());
  if(auto v = obj.tryGet("DBC"))
    n.dbcPath = QString::fromStdString(v->toStdString());
  if(auto v = obj.tryGet("NodeIdOffset"))
    n.nodeIdOffset = v->toInt();
  if(auto v = obj.tryGet("Float32Override"))
    n.float32Override = v->toBool();
  if(auto v = obj.tryGet("FD"))
    n.fd = v->toBool();
  if(auto v = obj.tryGet("FilterToDatabase"))
    n.filterToDatabase = v->toBool();
}
#endif
