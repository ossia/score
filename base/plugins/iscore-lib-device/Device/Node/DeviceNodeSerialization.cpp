#include <Device/Node/DeviceNode.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VariantSerialization.hpp>

template <>
ISCORE_LIB_DEVICE_EXPORT void
DataStreamReader::read(const Device::DeviceExplorerNode& n)
{
  readFrom(n.m_data);
  insertDelimiter();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
DataStreamWriter::writeTo(Device::DeviceExplorerNode& n)
{
  writeTo(n.m_data);
  checkDelimiter();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
JSONObjectReader::readFrom(const Device::DeviceExplorerNode& n)
{
  readFrom(n.m_data);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
JSONObjectWriter::writeTo(Device::DeviceExplorerNode& n)
{
  writeTo(n.m_data);
}
