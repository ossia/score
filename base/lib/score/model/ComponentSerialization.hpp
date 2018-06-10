#pragma once
#include <score/model/Component.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <type_traits>

namespace score
{

/**
 * @brief A component that keeps the data of all serialized components.
 *
 * Used only when reloading, because components have to be instantiated after
 * their entity.
 */
class SCORE_LIB_BASE_EXPORT DataStreamSerializedComponents
    : public score::Component
{
  COMMON_COMPONENT_METADATA("a0c8de61-c18f-4aca-8b21-cf71451f4970")
public:
  static const constexpr bool is_unique = true;
  DataStreamSerializedComponents(
      const Id<score::Component>& id,
      DataStreamComponents obj,
      QObject* parent);
  virtual ~DataStreamSerializedComponents();

  //! Returns true if all components could be loaded
  bool deserializeRemaining(score::Components& comps, QObject* entity);

  bool finished() const
  {
    return data.empty();
  }

  DataStreamComponents data;
};

class SCORE_LIB_BASE_EXPORT JSONSerializedComponents : public score::Component
{
  COMMON_COMPONENT_METADATA("37939615-7165-4bb0-9cdd-e7426153d222")
public:
  static const constexpr bool is_unique = true;
  JSONSerializedComponents(
      const Id<score::Component>& id, JSONComponents obj, QObject* parent);

  virtual ~JSONSerializedComponents();

  //! Returns true if all components could be loaded
  bool deserializeRemaining(score::Components& comps, QObject* entity);

  bool finished() const
  {
    return data.empty();
  }

  JSONComponents data;
};

class SCORE_LIB_BASE_EXPORT SerializableComponent
    : public score::Component
    , public score::SerializableInterface<score::SerializableComponent>
{
public:
  using score::Component::Component;

  template <typename Vis>
  SerializableComponent(Vis& vis, QObject* parent)
      : score::Component{vis, parent}
  {
  }

  virtual InterfaceKey interfaceKey() const = 0;
};

struct SCORE_LIB_BASE_EXPORT SerializableComponentFactory
    : public score::InterfaceBase
{
  SCORE_INTERFACE(SerializableComponentFactory, "ffafadc2-0ce7-45d8-b673-d9238c37d018")
public:
  ~SerializableComponentFactory() override;
  virtual score::SerializableComponent* make(
      const Id<score::Component>& id,
      const score::DocumentContext& ctx,
      QObject* parent)
      = 0;

  virtual score::SerializableComponent* load(
      const VisitorVariant& vis,
      const score::DocumentContext& ctx,
      QObject* parent)
      = 0;
};

struct SCORE_LIB_BASE_EXPORT SerializableComponentFactoryList
    : public score::InterfaceList<SerializableComponentFactory>
{
  using object_type = score::SerializableComponent;
  ~SerializableComponentFactoryList();
  score::SerializableComponent* loadMissing(
      const VisitorVariant& vis,
      const score::DocumentContext& ctx,
      QObject* parent) const;
};

template <typename System_T>
class GenericSerializableComponent : public score::SerializableComponent
{
public:
  template <typename... Args>
  GenericSerializableComponent(System_T& sys, Args&&... args)
      : score::SerializableComponent{std::forward<Args>(args)...}
      , m_system{sys}
  {
  }

  System_T& system() const
  {
    return m_system;
  }

private:
  System_T& m_system;
};

struct serializable_tag
{
};
struct not_serializable_tag
{
};

template <typename T, typename = void>
struct is_component_serializable
{
  using type = score::not_serializable_tag;
  static constexpr bool value = false;
};

template <typename T>
struct is_component_serializable<
    T,
    std::enable_if_t<std::is_base_of<score::SerializableComponent, T>::value>>
{
  using type = score::serializable_tag;
  static constexpr bool value = true;
};

template <typename Component_T, typename Fun>
auto deserialize_component(score::Components& comps, Fun f)
{
  if (auto datastream_ser
      = findComponent<DataStreamSerializedComponents>(comps))
  {
    auto& data = datastream_ser->data;
    auto it = data.find(Component_T::static_key());
    if (it != data.end())
    {
      DataStream::Deserializer des{it->second};
      auto res = f(des);
      if (res)
      {
        data.erase(it);
      }
      return res;
    }
  }
  else if (auto json_ser = findComponent<JSONSerializedComponents>(comps))
  {
    auto& data = json_ser->data;
    auto it = data.find(Component_T::static_key());
    if (it != data.end())
    {
      JSONObject::Deserializer des{it->second};
      auto res = f(des);
      if (res)
      {
        data.erase(it);
      }
      return res;
    }
  }
}

SCORE_LIB_BASE_EXPORT
void deserializeRemainingComponents(score::Components& comps, QObject* obj);
}
