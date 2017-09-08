#pragma once
#include <QMetaType>
#include <score_lib_device_export.h>
#include <memory>

namespace ossia
{
struct domain;
}

namespace Device
{
struct SCORE_LIB_DEVICE_EXPORT Domain
{
  Q_GADGET
public:
  Domain() noexcept;
  Domain(const Domain& other) noexcept;
  Domain(Domain&& other) noexcept;
  Domain& operator=(const Domain& other) noexcept;
  Domain& operator=(Domain&& other) noexcept;
  ~Domain();

  Domain(const ossia::domain&) noexcept;
  Domain& operator=(const ossia::domain&) noexcept;

  operator const ossia::domain&() const noexcept;
  operator ossia::domain&() noexcept;

  bool operator==(const Device::Domain& other) const noexcept;
  bool operator!=(const Device::Domain& other) const noexcept;

  const ossia::domain& get() const noexcept;
  ossia::domain& get() noexcept;

private:
  std::unique_ptr<ossia::domain> domain;
};
}

Q_DECLARE_METATYPE(Device::Domain)
