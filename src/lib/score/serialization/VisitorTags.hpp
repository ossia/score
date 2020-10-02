#pragma once
#include <score/serialization/IsTemplate.hpp>

#include <cinttypes>
#include <limits>

#include <type_traits>

//! \see SerializableInterface
template <typename T>
using enable_if_abstract_base =
    typename std::enable_if_t<std::decay<T>::type::is_abstract_base_tag::value>;

template <class, class Enable = void>
struct is_abstract_base : std::false_type
{
};

template <class T>
struct is_abstract_base<T, enable_if_abstract_base<T>> : std::true_type
{
};

//! \see IdentifiedObject
template <typename T>
using enable_if_object = typename std::enable_if_t<T::identified_object_tag>;

template <class, class Enable = void>
struct is_identified_object : std::false_type
{
};

template <class T>
struct is_identified_object<T, enable_if_object<T>> : std::true_type
{
};

//! \see Entity
template <typename T>
using enable_if_entity = typename std::enable_if_t<T::entity_tag>;

template <class, class Enable = void>
struct is_entity : std::false_type
{
};

template <class T>
struct is_entity<T, enable_if_entity<T>> : std::true_type
{
};

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
struct base_kind<T, std::enable_if_t<!std::is_same_v<T, typename T::base_type>>>
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

template <typename T, typename = void>
struct serialization_tag
{
  using type = visitor_default_tag;
};

template <typename T>
struct serialization_tag<
    T,
    std::enable_if_t<
        !is_identified_object<T>::value && is_abstract_base<T>::value && !is_custom_serialized<T>::value>>
{
  using type = visitor_abstract_tag;
};

template <typename T>
struct serialization_tag<
    T,
    std::enable_if_t<
        is_identified_object<T>::value && !is_entity<T>::value && !is_abstract_base<T>::value && !is_custom_serialized<T>::value>>
{
  using type = visitor_object_tag;
};

template <typename T>
struct serialization_tag<
    T,
    std::enable_if_t<
        is_identified_object<T>::value && !is_entity<T>::value && is_abstract_base<T>::value && !is_custom_serialized<T>::value>>
{
  using type = visitor_abstract_object_tag;
};

template <typename T>
struct serialization_tag<
    T,
    std::enable_if_t<
        is_entity<T>::value && !is_abstract_base<T>::value && !is_custom_serialized<T>::value>>
{
  using type = visitor_entity_tag;
};

template <typename T>
struct serialization_tag<
    T,
    std::enable_if_t<
        is_entity<T>::value && is_abstract_base<T>::value && !is_custom_serialized<T>::value>>
{
  using type = visitor_abstract_entity_tag;
};

template <typename T>
struct serialization_tag<
    T,
    std::enable_if_t<
        is_template<T>::value && !is_abstract_base<T>::value && !is_identified_object<T>::value && !is_custom_serialized<T>::value>>
{
  using type = visitor_template_tag;
};

template <typename T>
struct serialization_tag<T, std::enable_if_t<is_custom_serialized<T>::value>>
{
  using type = visitor_template_tag;
};

template <typename T>
struct serialization_tag<T, std::enable_if_t<std::is_enum<T>::value>>
{
  using type = visitor_enum_tag;
};

template <typename T>
struct check_enum_size
{
  using type_limits = std::numeric_limits<std::underlying_type_t<T>>;
  using int_limits = std::numeric_limits<int32_t>;
  static constexpr bool value
      = type_limits::min() >= int_limits::min() && type_limits::max() <= int_limits::max();
};
