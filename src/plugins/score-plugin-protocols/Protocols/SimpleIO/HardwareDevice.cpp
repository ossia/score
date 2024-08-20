#include "HardwareDevice.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::SimpleIO::HardwareDevice)

namespace Protocols::SimpleIO
{
HardwareDevice::~HardwareDevice() = default;
HardwareDeviceFactory::~HardwareDeviceFactory() = default;

HardwareDevice::HardwareDevice(QObject* parent) noexcept
    : QObject{parent}
{
}

HardwareDevice::HardwareDevice(DataStream::Deserializer& vis, QObject* parent)
    : QObject{parent}
{
  vis.writeTo(*this);
}

HardwareDevice::HardwareDevice(JSONObject::Deserializer& vis, QObject* parent)
    : QObject{parent}
{
  vis.writeTo(*this);
}
}

template <>
void DataStreamReader::read(const Protocols::SimpleIO::HardwareDevice& segmt)
{
  // m_stream << segmt.previous() << segmt.following() << segmt.start() << segmt.end();
}

template <>
void DataStreamWriter::write(Protocols::SimpleIO::HardwareDevice& segmt)
{
  // m_stream >> segmt.m_previous >> segmt.m_following >> segmt.m_start >> segmt.m_end;
}

template <>
void JSONReader::read(const Protocols::SimpleIO::HardwareDevice& segmt)
{
  /*
  using namespace Curve;
  
  obj[strings.Previous] = segmt.previous();
  obj[strings.Following] = segmt.following();
  obj[strings.Start] = segmt.start();
  obj[strings.End] = segmt.end();
*/
}

template <>
void JSONWriter::write(Protocols::SimpleIO::HardwareDevice& segmt)
{
  /*
  using namespace Curve;
  segmt.m_previous <<= obj[strings.Previous];
  segmt.m_following <<= obj[strings.Following];
  segmt.m_start <<= obj[strings.Start];
  segmt.m_end <<= obj[strings.End];
*/
}
