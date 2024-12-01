#pragma once
#include <score_lib_device_export.h>

#include <string>
namespace Device
{
class DeviceInterface;
class SCORE_LIB_DEVICE_EXPORT CodeWriter
{
public:
  explicit CodeWriter(const DeviceInterface& p) noexcept;
  virtual ~CodeWriter();

  CodeWriter() = delete;
  CodeWriter(const CodeWriter&) = delete;
  CodeWriter(CodeWriter&&) = delete;
  CodeWriter& operator=(const CodeWriter&) = delete;
  CodeWriter& operator=(CodeWriter&&) = delete;

  virtual std::string init() = 0;
  virtual std::string readPins() = 0;
  virtual std::string readOSC() = 0;
  virtual std::string writePins() = 0;
  virtual std::string writeOSC() = 0;

protected:
  const Device::DeviceInterface& self;
};
}
