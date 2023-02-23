#pragma once
#include <score/serialization/IsTemplate.hpp>

#include <cinttypes>
#include <limits>
#include <type_traits>

//! \see SerializableInterface
template <typename T>
concept abstract_base = std::decay_t<T>::is_abstract_base_tag::value;

//! \see IdentifiedObject
template <typename T>
concept identified_object = T::identified_object_tag;

//! \see Entity
template <typename T>
concept identified_entity = T::entity_tag;

struct has_no_base
{
};
struct has_base
{
};

template <typename T, typename U = void>
struct base_kind
{
  using type = has_no_base;
  static constexpr bool value = false;
};

template <typename T>
struct base_kind<T, std::enable_if_t<!std::is_same_v<T, typename T::score_base_type>>>
{
  using type = has_base;
  static constexpr bool value = true;
};

//! Classes that only inherit from score::SerializableInterface
struct visitor_abstract_tag
{
};

//! Classes that only inherit from score::IdentifiedObject
struct visitor_object_tag
{
};

//! Classes that inherit from score::IdentifiedObject and
//! score::SerializableInterface
struct visitor_abstract_object_tag
{
};

//! Classes that only inherit from score::Entity
struct visitor_entity_tag
{
};

//! Classes that inherit from score::Entity and score::SerializableInterface
struct visitor_abstract_entity_tag
{
};

//! Template classes
struct visitor_template_tag
{
};

//! Enums
struct visitor_enum_tag
{
};

//! All the other types.
struct visitor_default_tag
{
};

template <typename T>
struct serialization_tag
{
  using type = visitor_default_tag;
};

template <typename T>
  requires(!identified_object<T> && abstract_base<T> && !is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_abstract_tag;
};

template <typename T>
  requires(
      identified_object<T> && !identified_entity<T> && !abstract_base<T>
      && !is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_object_tag;
};

template <typename T>
  requires(
      identified_object<T> && !identified_entity<T> && abstract_base<T>
      && !is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_abstract_object_tag;
};

template <typename T>
  requires(identified_entity<T> && !abstract_base<T> && !is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_entity_tag;
};

template <typename T>
  requires(identified_entity<T> && abstract_base<T> && !is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_abstract_entity_tag;
};

template <typename T>
  requires(
      is_template<T>::value
      && !abstract_base<T> && !identified_object<T> && !is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_template_tag;
};

template <typename T>
  requires(is_custom_serialized<T>::value)
struct serialization_tag<T>
{
  using type = visitor_template_tag;
};

template <typename T>
  requires(std::is_enum<T>::value)
struct serialization_tag<T>
{
  using type = visitor_enum_tag;
};

template <typename T>
struct check_enum_size
{
  using type_limits = std::numeric_limits<std::underlying_type_t<T>>;
  using int_limits = std::numeric_limits<int32_t>;
  static constexpr bool value = type_limits::min() >= int_limits::min()
                                && type_limits::max() <= int_limits::max();
};
