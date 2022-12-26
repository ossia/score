// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Device/Node/DeviceNode.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamReader::read(const Device::DeviceExplorerNode& n)
{
  readFrom(n.m_data);
  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void DataStreamWriter::write(Device::DeviceExplorerNode& n)
{
  writeTo(n.m_data);
  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONReader::read(const Device::DeviceExplorerNode& n)
{
  switch(n.m_data.which().index())
  {
    case Device::Node::variant_type::index_of<InvisibleRootNode>().index():
      // invisible root node
      break;
    case Device::Node::variant_type::index_of<Device::DeviceSettings>().index():
      obj[strings.Device] = *n.m_data.target<Device::DeviceSettings>();
      break;
    case Device::Node::variant_type::index_of<Device::AddressSettings>().index():
      obj[strings.Address] = *n.m_data.target<Device::AddressSettings>();
      break;
    default:
      SCORE_ABORT;
  }
}

template <>
SCORE_LIB_DEVICE_EXPORT void JSONWriter::write(Device::DeviceExplorerNode& n)
{
  if(auto it = obj.tryGet(strings.Device))
  {
    Device::DeviceSettings res;
    res <<= *it;
    n = std::move(res);
  }
  else if(auto it = obj.tryGet(strings.Address))
  {
    Device::AddressSettings res;
    res <<= *it;
    n = std::move(res);
  }
  else if(auto it = obj.tryGet("AddressSettings"))
  {
    Device::AddressSettings res;
    res <<= *it;
    n = std::move(res);
  }
}
