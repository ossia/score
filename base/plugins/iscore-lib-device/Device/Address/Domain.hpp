#pragma once
#include <QMetaType>
#include <iscore_lib_device_export.h>
#include <memory>

namespace ossia
{
namespace net
{
struct domain;
}
}

namespace Device
{
struct ISCORE_LIB_DEVICE_EXPORT Domain
{
  Q_GADGET
public:
  Domain() noexcept;
  Domain(const Domain& other) noexcept;
  Domain(Domain&& other) noexcept;
  Domain& operator=(const Domain& other) noexcept;
  Domain& operator=(Domain&& other) noexcept;
  ~Domain();

  Domain(const ossia::net::domain&) noexcept;
  Domain& operator=(const ossia::net::domain&) noexcept;

  operator const ossia::net::domain&() const noexcept;
  operator ossia::net::domain&() noexcept;

  bool operator==(const Device::Domain& other) const noexcept;
  bool operator!=(const Device::Domain& other) const noexcept;

  const ossia::net::domain& get() const noexcept;
  ossia::net::domain& get() noexcept;

private:
  std::unique_ptr<ossia::net::domain> domain;
};
}

Q_DECLARE_METATYPE(Device::Domain)
