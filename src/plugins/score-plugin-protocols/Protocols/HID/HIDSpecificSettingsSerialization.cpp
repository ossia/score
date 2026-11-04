#include "HIDSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::HIDSpecificSettings& n)
{
  m_stream << n.path << n.serial << n.product << n.vendorId << n.productId
           << n.interfaceNumber;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::HIDSpecificSettings& n)
{
  m_stream >> n.path >> n.serial >> n.product >> n.vendorId >> n.productId
      >> n.interfaceNumber;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::HIDSpecificSettings& n)
{
  obj["Path"] = n.path;
  obj["Serial"] = n.serial;
  obj["Product"] = n.product;
  obj["VendorId"] = n.vendorId;
  obj["ProductId"] = n.productId;
  obj["Interface"] = n.interfaceNumber;
}

template <>
void JSONWriter::write(Protocols::HIDSpecificSettings& n)
{
  n.path = obj["Path"].toString();
  n.serial = obj["Serial"].toString();
  n.product = obj["Product"].toString();
  n.vendorId = obj["VendorId"].toInt();
  n.productId = obj["ProductId"].toInt();
  n.interfaceNumber = obj["Interface"].toInt();
}
