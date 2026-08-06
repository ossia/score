#pragma once
#include <score/plugins/SerializableInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

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
    const FactoryList_T& factories, DataStream::Deserializer& des, Args&&... args) ->
    typename FactoryList_T::object_type*
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  // Deserialize the interface identifier
  typename FactoryList_T::factory_type::ConcreteKey k;
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    TSerializer<DataStream, typename FactoryList_T::factory_type::ConcreteKey>::writeTo(
        sub, k);

    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    // Get the factory
    if(auto concrete_factory = factories.get(k))
    {
      // Create the object
      auto obj = concrete_factory->load(sub.toVariant(), std::forward<Args>(args)...);

      SCORE_DEBUG_CHECK_DELIMITER2(sub);

      return obj;
    }
  }
  catch(...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" version of
  // it. The key is handed over so that the stand-in can keep the identity of
  // what it replaces and save it back unchanged.
  return factories.loadMissing(k, sub.toVariant(), std::forward<Args>(args)...);
}

template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories, DataStream::Deserializer&& des, Args&&... args) ->
    typename FactoryList_T::object_type*
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  // Deserialize the interface identifier
  typename FactoryList_T::factory_type::ConcreteKey k;
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    TSerializer<DataStream, typename FactoryList_T::factory_type::ConcreteKey>::writeTo(
        sub, k);

    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    // Get the factory
    if(auto concrete_factory = factories.get(k))
    {
      // Create the object
      auto obj = concrete_factory->load(sub.toVariant(), std::forward<Args>(args)...);

      SCORE_DEBUG_CHECK_DELIMITER2(sub);

      return obj;
    }
  }
  catch(...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" version of
  // it. The key is handed over so that the stand-in can keep the identity of
  // what it replaces and save it back unchanged.
  return factories.loadMissing(k, sub.toVariant(), std::forward<Args>(args)...);
}

template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories, JSONObject::Deserializer& des, Args&&... args) ->
    typename FactoryList_T::object_type*
{
  // Deserialize the interface identifier
  typename FactoryList_T::factory_type::ConcreteKey k;
  try
  {
    JSONWriter wr{des.obj[des.strings.uuid]};
    TSerializer<JSONObject, typename FactoryList_T::factory_type::ConcreteKey>::writeTo(
        wr, k);

    // Get the factory
    if(auto concrete_factory = factories.get(k))
    {
      // Create the object
      return concrete_factory->load(des.toVariant(), std::forward<Args>(args)...);
    }
  }
  catch(...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" version of
  // it. The key is handed over so that the stand-in can keep the identity of
  // what it replaces and save it back unchanged.
  return factories.loadMissing(k, des.toVariant(), std::forward<Args>(args)...);
}

template <typename FactoryList_T, typename... Args>
auto deserialize_interface(
    const FactoryList_T& factories, JSONObject::Deserializer&& des, Args&&... args) ->
    typename FactoryList_T::object_type*
{
  // Deserialize the interface identifier
  typename FactoryList_T::factory_type::ConcreteKey k;
  try
  {
    JSONWriter wr{des.obj[des.strings.uuid]};
    TSerializer<JSONObject, typename FactoryList_T::factory_type::ConcreteKey>::writeTo(
        wr, k);

    // Get the factory
    if(auto concrete_factory = factories.get(k))
    {
      // Create the object
      return concrete_factory->load(des.toVariant(), std::forward<Args>(args)...);
    }
  }
  catch(...)
  {
  }

  // If the object could not be loaded, we try to load a "missing" version of
  // it. The key is handed over so that the stand-in can keep the identity of
  // what it replaces and save it back unchanged.
  return factories.loadMissing(k, des.toVariant(), std::forward<Args>(args)...);
}
