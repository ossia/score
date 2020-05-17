#pragma once
#include <score/model/Component.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score
{

/**
 * @brief component Fetch a Component from Components by type
 * @return The component.
 *
 * The component must have a member `static const constexpr bool is_unique =
 * true` in order to use this function.
 *
 * This guarantees that there will be a single component of a given type in the
 * Components.
 */
template <typename T>
T& component(const score::Components& c)
{
  static_assert(T::is_unique, "Components must be unique to use getComponent");

  auto it = ossia::find_if(c, [](auto& other) { return other.key_match(T::static_key()); });

  SCORE_ASSERT(it != c.end());
  return static_cast<T&>(*it);
}

/**
 * @brief findComponent Tryies to fetch a Component from Components by type.
 *
 * This works similarly to \ref components ; instead of aborting,
 * it returns a null pointer if the component does not exist.
 * @see \ref components
 */
template <typename T>
T* findComponent(const score::Components& c) noexcept
{
  static_assert(T::is_unique, "Components must be unique to use getComponent");

  auto it = ossia::find_if(c, [](auto& other) { return other.key_match(T::static_key()); });

  if (it != c.end())
  {
    return static_cast<T*>(&*it);
  }
  else
  {
    return (T*)nullptr;
  }
}
}
