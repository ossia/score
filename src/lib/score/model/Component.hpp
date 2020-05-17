#pragma once
#include <score/model/EntityMap.hpp>
#include <score/plugins/UuidKey.hpp>

#if defined(SCORE_SERIALIZABLE_COMPONENTS)
#include <score/tools/std/HashMap.hpp>

#include <QByteArray>
#endif

#include <utility>
#include <verdigris>
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
 * \typedef Components
 * \brief A map tailored for storing of components.
 */
using Components = EntityMap<score::Component>;

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

  System_T& system() const noexcept { return m_system; }

private:
  System_T& m_system;
};

#if defined(SCORE_SERIALIZABLE_COMPONENTS)
class SerializableComponent;
using DataStreamComponents
    = score::hash_map<UuidKey<score::SerializableComponent>, QByteArray>;
using JSONComponents
    = score::hash_map<UuidKey<score::SerializableComponent>, QJsonObject>;
#endif
}

#if defined(SCORE_SERIALIZABLE_COMPONENTS)
#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__)
extern template class SCORE_LIB_BASE_EXPORT
    tsl::hopscotch_map<UuidKey<score::SerializableComponent>, QByteArray>;
extern template class SCORE_LIB_BASE_EXPORT
    tsl::hopscotch_map<UuidKey<score::SerializableComponent>, QJsonObject>;
#endif
#endif

/**
 * \macro ABSTRACT_COMPONENT_METADATA
 */
#define ABSTRACT_COMPONENT_METADATA(Type, Uuid)                        \
public:                                                                \
  using base_component_type = Type;                                    \
                                                                       \
  static MSVC_BUGGY_CONSTEXPR Component::Key static_key() noexcept \
  {                                                                    \
    return_uuid(Uuid);                                                 \
  }                                                                    \
                                                                       \
  static MSVC_BUGGY_CONSTEXPR bool base_key_match(                 \
      Component::Key other) noexcept                                   \
  {                                                                    \
    return static_key() == other;                                      \
  }                                                                    \
                                                                       \
private:

/**
 * \macro COMPONENT_METADATA
 */
#define COMPONENT_METADATA(Uuid)                                              \
public:                                                                       \
  static MSVC_BUGGY_CONSTEXPR Component::Key static_key() noexcept        \
  {                                                                           \
    return_uuid(Uuid);                                                        \
  }                                                                           \
                                                                              \
  Component::Key key() const noexcept final override { return static_key(); } \
                                                                              \
  bool key_match(Component::Key other) const noexcept final override          \
  {                                                                           \
    return static_key() == other                                              \
           || base_component_type::base_key_match(other);                     \
  }                                                                           \
                                                                              \
private:

/**
 * \macro COMMON_COMPONENT_METADATA
 */
#define COMMON_COMPONENT_METADATA(Uuid)                                       \
public:                                                                       \
  static MSVC_BUGGY_CONSTEXPR Component::Key static_key() noexcept        \
  {                                                                           \
    return_uuid(Uuid);                                                        \
  }                                                                           \
                                                                              \
  Component::Key key() const noexcept final override { return static_key(); } \
                                                                              \
  bool key_match(Component::Key other) const noexcept final override          \
  {                                                                           \
    return static_key() == other;                                             \
  }                                                                           \
                                                                              \
private:
