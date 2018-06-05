#pragma once
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

namespace score
{
/**
 * @brief Generic serialization method for abstract classes.
 *
 * The problem is reloading polymorphic classes : we have to save
 * the identifier of the factory used to instantiate them.
 * <br>
 * Base classes should inherit from SerializableInterface, and provide only
 * serialization code for their own data in Visitor<Reader<...>> and
 * Visitor<Writer<...>>. <br> Likewise, subclasses should only save their own
 * data. These classes ensure that everything will be saved in the correct
 * order. <br> See visitor_abstract_tag
 */
template <typename T>
class SerializableInterface
{
public:
  using key_type = UuidKey<T>;
  using is_abstract_base_tag = std::integral_constant<bool, true>;

  SerializableInterface() = default;
  virtual ~SerializableInterface() = default;
  virtual UuidKey<T> concreteKey() const = 0;

  virtual void serialize_impl(const VisitorVariant& vis) const
  {
  }
};
}

template <typename Type>
Type deserialize_key(JSONObject::Deserializer& des)
{
  return fromJsonValue<score::uuid_t>(des.obj[des.strings.uuid]);
}

template <typename Type>
Type deserialize_key(DataStream::Deserializer& des)
{
  score::uuid_t uid;
  des.writeTo(uid);
  return uid;
}
/**
 * @brief deserialize_interface Reload a polymorphic type
 * @param factories The list of factories where the correct factory is.
 * @param des The deserializer instance
 * @param args Used to provide additional arguments for the factory "load"
 * method (for instance, a context).
 *
 * @return An instance of the object if a factory was found. Else, if there is
 * one available, the "missing factory" element. There is no guarantee that the
 * return value points to a valid object, it should always be checked.
 */
template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories,
    DataStream::Deserializer& des,
    Args&&... args) -> typename FactoryList_T::object_type*
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  // Deserialize the interface identifier
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    auto k
        = deserialize_key<typename FactoryList_T::factory_type::ConcreteKey>(
            sub);

    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    // Get the factory
    if (auto concrete_factory = factories.get(k))
    {
      // Create the object
      auto obj = concrete_factory->load(
          sub.toVariant(), std::forward<Args>(args)...);

      SCORE_DEBUG_CHECK_DELIMITER2(sub);

      return obj;
    }
  }
  catch (...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" verson of
  // it.
  return factories.loadMissing(sub.toVariant(), std::forward<Args>(args)...);
}

template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories,
    DataStream::Deserializer&& des,
    Args&&... args) -> typename FactoryList_T::object_type*
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  // Deserialize the interface identifier
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    auto k
        = deserialize_key<typename FactoryList_T::factory_type::ConcreteKey>(
            sub);

    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    // Get the factory
    if (auto concrete_factory = factories.get(k))
    {
      // Create the object
      auto obj = concrete_factory->load(
          sub.toVariant(), std::forward<Args>(args)...);

      SCORE_DEBUG_CHECK_DELIMITER2(sub);

      return obj;
    }
  }
  catch (...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" verson of
  // it.
  return factories.loadMissing(sub.toVariant(), std::forward<Args>(args)...);
}

template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories,
    JSONObject::Deserializer& des,
    Args&&... args) -> typename FactoryList_T::object_type*
{
  // Deserialize the interface identifier
  try
  {
    auto k
        = deserialize_key<typename FactoryList_T::factory_type::ConcreteKey>(
            des);

    // Get the factory
    if (auto concrete_factory = factories.get(k))
    {
      // Create the object
      return concrete_factory->load(
          des.toVariant(), std::forward<Args>(args)...);
    }
  }
  catch (...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" verson of
  // it.
  return factories.loadMissing(des.toVariant(), std::forward<Args>(args)...);
}

template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories,
    JSONObject::Deserializer&& des,
    Args&&... args) -> typename FactoryList_T::object_type*
{
  // Deserialize the interface identifier
  try
  {
    auto k
        = deserialize_key<typename FactoryList_T::factory_type::ConcreteKey>(
            des);

    // Get the factory
    if (auto concrete_factory = factories.get(k))
    {
      // Create the object
      return concrete_factory->load(
          des.toVariant(), std::forward<Args>(args)...);
    }
  }
  catch (...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" verson of
  // it.
  return factories.loadMissing(des.toVariant(), std::forward<Args>(args)...);
}

/**
 * @macro MODEL_METADATA_IMPL Provides default implementations of methods of
 * SerializableInterface.
 */
#define MODEL_METADATA_IMPL(Model_T)                                  \
  static key_type static_concreteKey()                                \
  {                                                                   \
    return Metadata<ConcreteKey_k, Model_T>::get();                   \
  }                                                                   \
  key_type concreteKey() const override                               \
  {                                                                   \
    return static_concreteKey();                                      \
  }                                                                   \
  void serialize_impl(const VisitorVariant& vis) const override       \
  {                                                                   \
    score::serialize_dyn(vis, *this);                                 \
  }
