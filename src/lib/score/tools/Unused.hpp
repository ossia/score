#pragma once
struct unused_t
{
  template <typename... Args>
  constexpr unused_t(Args&&...) noexcept
  {
  }
};
