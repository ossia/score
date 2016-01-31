#pragma once
#include <type_traits>
/**
 *
 * This file contains the base types for the serialization mechanism
 * in i-score.
 */

class AbstractVisitor
{
};

template<typename VisitorType>
class Visitor : public AbstractVisitor
{
};

template<typename T>
class Reader
{
};

template<typename T>
class Writer
{
};

template<typename T>
using Serializer = Visitor<Reader<T>>;

template<typename T>
using Deserializer = Visitor<Writer<T>>;

template<typename Serializer_T, typename Enable, typename... Args>
struct TSerializer;

template<typename Serializer_T, typename T>
struct AbstractSerializer;
template<typename Serializer_T, typename T>
struct ConcreteSerializer;

using SerializationIdentifier = int;

/**
 * @brief The VisitorVariant struct
 *
 * Allows to pass visitor of multiple types in a function,
 * which is necessary when crossing plug-ins bounds if we don't
 * want to break their interface the day we change the available
 * serialization formats.
 */
struct VisitorVariant
{
        AbstractVisitor& visitor;
        const SerializationIdentifier identifier;
};

/**
 * Will allow a template to be selected if the given member
 * is a deserializer.
 */
template<typename DeserializerVisitor>
using enable_if_deserializer = typename std::enable_if_t<std::decay<DeserializerVisitor>::type::is_visitor_tag::value>;


// Declaration of common friends for classes that serialize themselves
#define ISCORE_SERIALIZE_FRIENDS(Type, Serializer) \
    friend void Visitor<Reader< Serializer >>::readFrom< Type > (const Type &); \
    friend void Visitor<Reader< Serializer >>::readFrom_impl< Type > (const Type &); \
    friend void Visitor<Writer< Serializer >>::writeTo< Type > (Type &);


// Inherit from this to have
// the type treated as a value in the serialization context. Useful
// for choice between JSONObject / JSONValue
template<typename T>
struct is_value_tag {
        static const constexpr bool value = false;
};
#define ISCORE_DECL_VALUE_TYPE(Type) \
    template<> struct is_value_tag<Type> { static const constexpr bool value = true; };

template<typename T>
struct is_value_t
{
    static const constexpr bool value{
        std::is_arithmetic<T>::value
     || std::is_enum<T>::value
     || is_value_tag<T>::value
    };
};

template <class>
struct is_template : std::false_type {};

template <template <class...> class T, typename ...Args>
struct is_template<T<Args...>> : std::true_type {};

// see SerializableInterface
template<typename T>
using enable_if_abstract_base = typename std::enable_if_t<std::decay<T>::type::is_abstract_base_tag::value>;

template <class, class Enable = void>
struct is_abstract_base : std::false_type {};

template <class T>
struct is_abstract_base<T, enable_if_abstract_base<T> > : std::true_type {};

template<typename T>
using enable_if_concrete = typename std::enable_if_t<std::decay<T>::type::is_concrete_tag::value>;

template <class, class Enable = void>
struct is_concrete : std::false_type {};

template <class T>
struct is_concrete<T, enable_if_concrete<T> > : std::true_type {};


