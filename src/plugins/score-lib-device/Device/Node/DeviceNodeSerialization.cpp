// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Device/Node/DeviceNode.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamReader::read(const Device::DeviceExplorerNode& n)
{
  readFrom(n.m_data);
  insertDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
DataStreamWriter::write(Device::DeviceExplorerNode& n)
{
  writeTo(n.m_data);
  checkDelimiter();
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectReader::read(const Device::DeviceExplorerNode& n)
{
  readFrom(n.m_data);
}

template <>
SCORE_LIB_DEVICE_EXPORT void
JSONObjectWriter::write(Device::DeviceExplorerNode& n)
{
  writeTo(n.m_data);
}
