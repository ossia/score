#include "Domain.hpp"
#include <ossia/network/domain/domain.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
namespace Device
{

Domain::Domain() noexcept : domain{std::make_unique<ossia::net::domain>()}
{
}

Domain::Domain(const Domain& other) noexcept
    : domain{std::make_unique<ossia::net::domain>(*other.domain)}
{
}

Domain::Domain(Domain&& other) noexcept : domain{std::move(other.domain)}
{
  other.domain = std::make_unique<ossia::net::domain>();
}

Domain& Domain::operator=(const Domain& other) noexcept
{
  *domain = *other.domain;
  return *this;
}

Domain& Domain::operator=(Domain&& other) noexcept
{
  *domain = std::move(*other.domain);
  return *this;
}

Domain::~Domain()
{
}

Domain::Domain(const ossia::net::domain& other) noexcept
    : domain{std::make_unique<ossia::net::domain>(other)}
{
}

Domain& Domain::operator=(const ossia::net::domain& other) noexcept
{
  *domain = other;
  return *this;
}

bool Domain::operator==(const Domain& other) const noexcept
{
  return *domain == *other.domain;
}

bool Domain::operator!=(const Domain& other) const noexcept
{
  return *domain != *other.domain;
}

const ossia::net::domain& Domain::get() const noexcept
{
  return *domain;
}

ossia::net::domain& Domain::get() noexcept
{
  return *domain;
}

Domain::operator const ossia::net::domain&() const noexcept
{
  return *domain;
}

Domain::operator ossia::net::domain&() noexcept
{
  return *domain;
}
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<DataStream>>::read(const Device::Domain& var)
{
  readFrom(var.get());
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::Domain& var)
{
  writeTo(var.get());
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Device::Domain& var)
{
  readFrom(var.get());
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::Domain& var)
{
  writeTo(var.get());
}
