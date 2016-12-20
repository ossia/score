#pragma once
#include <ossia/detail/algorithms.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore/model/EntityMap.hpp>
namespace iscore
{
/**
 * @brief A component adds custom data to an Entity.
 *
 * \todo Document more.
 */
class ISCORE_LIB_BASE_EXPORT Component
    : public IdentifiedObject<iscore::Component>
{
public:
  using IdentifiedObject<iscore::Component>::IdentifiedObject;
  using Key = UuidKey<iscore::Component>;
  virtual Key key() const = 0;
  virtual bool key_match(Key other) const = 0;

  virtual ~Component();
};

/**
 * @brief A component that has a reference to a specific context object
 *
 * For instance if all components need to refer to a given DocumentPlugin instance,
 * one can inherit from this class.
 */
template <typename System_T>
class GenericComponent : public iscore::Component
{
public:
  template <typename... Args>
  GenericComponent(System_T& sys, Args&&... args)
      : iscore::Component{std::forward<Args>(args)...}
      , m_system{sys}
  {
  }

  System_T& system() const
  {
    return m_system;
  }

private:
  System_T& m_system;
};

/**
 * \typedef Components
 * \brief A map tailored for storing of components.
 */
using Components = EntityMap<iscore::Component>;


/**
 * @brief component Fetch a Component from Components by type
 * @return The component.
 *
 * The component must have a member `static const constexpr bool is_unique = true`
 * in order to use this function.
 *
 * This guarantees that there will be a single component of a given type in the Components.
 */
template <typename T>
auto& component(const iscore::Components& c)
{
  static_assert(T::is_unique, "Components must be unique to use getComponent");

  auto it = ossia::find_if(
      c, [](auto& other) { return other.key_match(T::static_key()); });

  ISCORE_ASSERT(it != c.end());
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
auto findComponent(const iscore::Components& c)
{
  static_assert(T::is_unique, "Components must be unique to use getComponent");

  auto it = ossia::find_if(
      c, [](auto& other) { return other.key_match(T::static_key()); });

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

/**
 * \macro ABSTRACT_COMPONENT_METADATA
 */
#define ABSTRACT_COMPONENT_METADATA(Type, Uuid)                             \
public:                                                                     \
  using base_component_type = Type;                                         \
                                                                            \
  static Q_DECL_RELAXED_CONSTEXPR Component::Key static_key()               \
  {                                                                         \
    return_uuid(Uuid);                                                      \
  }                                                                         \
                                                                            \
  static Q_DECL_RELAXED_CONSTEXPR bool base_key_match(Component::Key other) \
  {                                                                         \
    return static_key() == other;                                           \
  }                                                                         \
                                                                            \
private:

/**
 * \macro COMPONENT_METADATA
 */
#define COMPONENT_METADATA(Uuid)                              \
public:                                                       \
  static Q_DECL_RELAXED_CONSTEXPR Component::Key static_key() \
  {                                                           \
    return_uuid(Uuid);                                        \
  }                                                           \
                                                              \
  Component::Key key() const final override                   \
  {                                                           \
    return static_key();                                      \
  }                                                           \
                                                              \
  bool key_match(Component::Key other) const final override   \
  {                                                           \
    return static_key() == other                              \
           || base_component_type::base_key_match(other);     \
  }                                                           \
                                                              \
private:

/**
 * \macro COMMON_COMPONENT_METADATA
 */
#define COMMON_COMPONENT_METADATA(Uuid)                       \
public:                                                       \
  static Q_DECL_RELAXED_CONSTEXPR Component::Key static_key() \
  {                                                           \
    return_uuid(Uuid);                                        \
  }                                                           \
                                                              \
  Component::Key key() const final override                   \
  {                                                           \
    return static_key();                                      \
  }                                                           \
                                                              \
  bool key_match(Component::Key other) const final override   \
  {                                                           \
    return static_key() == other;                             \
  }                                                           \
                                                              \
private:
