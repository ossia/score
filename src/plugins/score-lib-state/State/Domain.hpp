#pragma once

#include <score_lib_state_export.h>

#include <memory>
#include <verdigris>

namespace ossia
{
struct domain;
}

namespace State
{
struct SCORE_LIB_STATE_EXPORT Domain
{
  // W_GADGET(Domain)
public:
  Domain() noexcept;
  Domain(const Domain& other) noexcept;
  Domain(Domain&& other) noexcept;
  Domain& operator=(const Domain& other) noexcept;
  Domain& operator=(Domain&& other) noexcept;
  ~Domain();

  Domain(const ossia::domain&) noexcept;
  Domain& operator=(const ossia::domain&) noexcept;

  operator const ossia::domain &() const noexcept;
  operator ossia::domain &() noexcept;

  bool operator==(const State::Domain& other) const noexcept;
  bool operator!=(const State::Domain& other) const noexcept;

  const ossia::domain& get() const noexcept;
  ossia::domain& get() noexcept;

private:
  std::unique_ptr<ossia::domain> domain;
};
}

Q_DECLARE_METATYPE(State::Domain)
W_REGISTER_ARGTYPE(State::Domain)
