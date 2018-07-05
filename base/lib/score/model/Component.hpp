#pragma once
#include <ossia/detail/algorithms.hpp>

#include <QByteArray>
#include <QJsonObject>
#include <score/model/EntityMap.hpp>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score/tools/std/HashMap.hpp>
#include <utility>
#include <wobjectdefs.h>
namespace score
{
struct lazy_init_t
{
};

/**
 * @brief A component adds custom data to an Entity.
 *
 * \todo Document more.
 */
class SCORE_LIB_BASE_EXPORT Component
    : public IdentifiedObject<score::Component>
{
  W_OBJECT(Component)
public:
  using IdentifiedObject<score::Component>::IdentifiedObject;
  using Key = UuidKey<score::Component>;
  virtual Key key() const noexcept = 0;
  virtual bool key_match(Key other) const noexcept = 0;

  ~Component() override;
};

/**
 * @brief A component that has a reference to a specific context object
 *
 * For instance if all components need to refer to a given DocumentPlugin
 * instance, one can inherit from this class.
 */
template <typename System_T>
class GenericComponent : public score::Component
{
public:
  template <typename... Args>
  GenericComponent(System_T& sys, Args&&... args) noexcept
      : score::Component{std::forward<Args>(args)...}, m_system{sys}
  {
  }

  System_T& system() const noexcept
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
using Components = EntityMap<score::Component>;

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

  auto it = ossia::find_if(
      c, [](auto& other) { return other.key_match(T::static_key()); });

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

class SerializableComponent;
using DataStreamComponents
    = score::hash_map<UuidKey<score::SerializableComponent>, QByteArray>;
using JSONComponents
    = score::hash_map<UuidKey<score::SerializableComponent>, QJsonObject>;
}
extern template class SCORE_LIB_BASE_EXPORT tsl::
    hopscotch_map<UuidKey<score::SerializableComponent>, QByteArray>;
extern template class SCORE_LIB_BASE_EXPORT tsl::
    hopscotch_map<UuidKey<score::SerializableComponent>, QJsonObject>;

/**
 * \macro ABSTRACT_COMPONENT_METADATA
 */
#define ABSTRACT_COMPONENT_METADATA(Type, Uuid)                             \
public:                                                                     \
  using base_component_type = Type;                                         \
                                                                            \
  static Q_DECL_RELAXED_CONSTEXPR Component::Key static_key() noexcept      \
  {                                                                         \
    return_uuid(Uuid);                                                      \
  }                                                                         \
                                                                            \
  static Q_DECL_RELAXED_CONSTEXPR bool base_key_match(Component::Key other) noexcept \
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
  static Q_DECL_RELAXED_CONSTEXPR Component::Key static_key() noexcept \
  {                                                           \
    return_uuid(Uuid);                                        \
  }                                                           \
                                                              \
  Component::Key key() const noexcept final  override                   \
  {                                                           \
    return static_key();                                      \
  }                                                           \
                                                              \
  bool key_match(Component::Key other) const noexcept final  override   \
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
  static Q_DECL_RELAXED_CONSTEXPR Component::Key static_key() noexcept \
  {                                                           \
    return_uuid(Uuid);                                        \
  }                                                           \
                                                              \
  Component::Key key() const noexcept final override          \
  {                                                           \
    return static_key();                                      \
  }                                                           \
                                                              \
  bool key_match(Component::Key other) const noexcept final  override   \
  {                                                           \
    return static_key() == other;                             \
  }                                                           \
                                                              \
private:
