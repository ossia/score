// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Domain.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/network/domain/domain.hpp>

#include <wobjectimpl.h>
// W_GADGET_IMPL(State::Domain)
namespace State
{

Domain::Domain() noexcept : domain{std::make_unique<ossia::domain>()} { }

Domain::Domain(const Domain& other) noexcept
    : domain{std::make_unique<ossia::domain>(*other.domain)}
{
}

Domain::Domain(Domain&& other) noexcept : domain{std::move(other.domain)}
{
  other.domain = std::make_unique<ossia::domain>();
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

Domain::~Domain() { }

Domain::Domain(const ossia::domain& other) noexcept
    : domain{std::make_unique<ossia::domain>(other)}
{
}

Domain& Domain::operator=(const ossia::domain& other) noexcept
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

const ossia::domain& Domain::get() const noexcept
{
  return *domain;
}

ossia::domain& Domain::get() noexcept
{
  return *domain;
}

Domain::operator const ossia::domain &() const noexcept
{
  return *domain;
}

Domain::operator ossia::domain &() noexcept
{
  return *domain;
}
}
