#pragma once
#include <score/serialization/IsTemplate.hpp>
/**
 *
 * This file contains the base types for the serialization mechanism
 * in score.
 */

class AbstractVisitor
{
};
template <typename Serializer_T, typename T, typename Enable = void>
struct TSerializer;

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
template <typename DeserializerVisitor>
using enable_if_deserializer =
    typename std::enable_if_t<std::decay<DeserializerVisitor>::type::is_visitor_tag::value>;

// Declaration of common friends for classes that serialize themselves
#define SCORE_SERIALIZE_FRIENDS    \
  friend class ::DataStreamReader; \
  friend class ::DataStreamWriter; \
  friend class ::JSONReader;       \
  friend class ::JSONWriter;

class DataStream;
class JSONObject;
// class JSONValue;
class DataStreamReader;
class DataStreamWriter;
class JSONReader;
class JSONWriter;
class DataStream
{
public:
  using Serializer = DataStreamReader;
  using Deserializer = DataStreamWriter;
  static constexpr SerializationIdentifier type() { return 2; }
};

class JSONObject
{
public:
  using Serializer = JSONReader;
  using Deserializer = JSONWriter;
  static constexpr SerializationIdentifier type() { return 1; }
};
