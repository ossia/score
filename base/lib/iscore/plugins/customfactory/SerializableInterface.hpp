#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>

#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

template <typename T>
struct AbstractSerializer<DataStream, T>
{
  static void readFrom(DataStream::Serializer& s, const T& obj)
  {
    // We save in a byte array so that
    // we have a chance to save it as-is and reload it later
    // if the plug-in is not found on the system.
    QByteArray b;
    DataStream::Serializer sub{&b};

    sub.readFrom(obj.concreteFactoryKey().impl());
    sub.readFrom_impl(obj);
    obj.serialize_impl(sub.toVariant());
    sub.insertDelimiter();

    s.stream() << std::move(b);
  }
};

template <typename T>
struct AbstractSerializer<JSONObject, T>
{
  static void readFrom(JSONObject::Serializer& s, const T& obj)
  {
    s.m_obj[s.strings.uuid] = toJsonValue(obj.concreteFactoryKey().impl());
    s.readFrom_impl(obj);
    obj.serialize_impl(s.toVariant());
  }
};

namespace iscore
{
// FIXME why is this not used everywhere
struct concrete
{
  using is_concrete_tag = std::integral_constant<bool, true>;
};
template <typename T>
class SerializableInterface
{
public:
  using key_type = UuidKey<T>;
  using is_abstract_base_tag = std::integral_constant<bool, true>;

  SerializableInterface() = default;
  virtual ~SerializableInterface() = default;
  virtual UuidKey<T> concreteFactoryKey() const = 0;

  virtual void serialize_impl(const VisitorVariant& vis) const
  {
  }
};
}

template <typename Type>
Type deserialize_key(Deserializer<JSONObject>& des)
{
  return fromJsonValue<iscore::uuid_t>(des.m_obj[des.strings.uuid]);
}

template <typename Type>
Type deserialize_key(Deserializer<DataStream>& des)
{
  iscore::uuid_t uid;
  des.writeTo(uid);
  return uid;
}

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
    auto k = deserialize_key<
        typename FactoryList_T::factory_type::ConcreteFactoryKey>(sub);

    // Get the factory
    if (auto concrete_factory = factories.get(k))
    {
      // Create the object
      auto obj = concrete_factory->load(
          sub.toVariant(), std::forward<Args>(args)...);

      sub.checkDelimiter();

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
    auto k = deserialize_key<
        typename FactoryList_T::factory_type::ConcreteFactoryKey>(des);

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

// This one does not have clone.
#define SERIALIZABLE_MODEL_METADATA_IMPL(Model_T)                     \
  static key_type static_concreteFactoryKey()                         \
  {                                                                   \
    return Metadata<ConcreteFactoryKey_k, Model_T>::get();            \
  }                                                                   \
  key_type concreteFactoryKey() const final override                  \
  {                                                                   \
    return static_concreteFactoryKey();                               \
  }                                                                   \
                                                                      \
  void serialize_impl(const VisitorVariant& vis) const final override \
  {                                                                   \
    serialize_dyn(vis, *this);                                        \
  }

#define MODEL_METADATA_IMPL(Model_T)                       \
  SERIALIZABLE_MODEL_METADATA_IMPL(Model_T)                \
  Model_T* clone(const id_type& newId, QObject* newParent) \
      const final override                                 \
  {                                                        \
    return new Model_T{*this, newId, newParent};           \
  }
