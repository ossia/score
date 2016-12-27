#pragma once
#include <type_traits>

#include <iscore/model/Entity.hpp>

class DataStream;

//! Classes that only inherit from iscore::SerializableInterface
struct visitor_abstract_tag {};

//! Classes that only inherit from iscore::IdentifiedObject
struct visitor_object_tag {};

//! Classes that inherit from iscore::IdentifiedObject and iscore::SerializableInterface
struct visitor_abstract_object_tag {};

//! Classes that only inherit from iscore::Entity
struct visitor_entity_tag {};

//! Classes that inherit from iscore::Entity and iscore::SerializableInterface
struct visitor_abstract_entity_tag {};

//! Template classes
struct visitor_template_tag {};

//! Enums
struct visitor_enum_tag {};

//! All the other types.
struct visitor_default_tag {};

template<typename T, typename = void>
struct serialization_tag
{
  using type = visitor_default_tag;
};

template<typename T>
struct serialization_tag<T,
      std::enable_if_t<
        !iscore::is_object<T>::value
      && is_abstract_base<T>::value>>
{
  using type = visitor_abstract_tag;
};

template<typename T>
struct serialization_tag<T,
      std::enable_if_t<
        iscore::is_object<T>::value
    && !iscore::is_entity<T>::value
    && !is_abstract_base<T>::value>>
{
  using type = visitor_object_tag;
};

template<typename T>
struct serialization_tag<T,
      std::enable_if_t<
        iscore::is_object<T>::value
    && !iscore::is_entity<T>::value
     && is_abstract_base<T>::value>>
{
  using type = visitor_abstract_object_tag;
};


template<typename T>
struct serialization_tag<T,
      std::enable_if_t<
        iscore::is_entity<T>::value
    && !is_abstract_base<T>::value>>
{
  using type = visitor_entity_tag;
};

template<typename T>
struct serialization_tag<T,
      std::enable_if_t<
        iscore::is_entity<T>::value
     && is_abstract_base<T>::value>>
{
  using type = visitor_abstract_entity_tag;
};

template<typename T>
struct serialization_tag<T,
      std::enable_if_t<
        is_template<T>::value
    && !is_abstract_base<T>::value
    >>
{
  using type = visitor_template_tag;
};

template<typename T>
struct serialization_tag<T,
      std::enable_if_t<std::is_enum<T>::value>>
{
  using type = visitor_enum_tag;
};
