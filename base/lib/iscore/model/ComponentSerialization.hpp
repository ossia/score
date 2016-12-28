#pragma once
#include <iscore/model/Component.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <type_traits>

namespace iscore
{

/**
 * @brief A component that keeps the data of all serialized components.
 *
 * Used only when reloading, because components have to be instantiated after
 * their entity.
 */
class ISCORE_LIB_BASE_EXPORT DataStreamSerializedComponents : public iscore::Component
{
    COMMON_COMPONENT_METADATA("a0c8de61-c18f-4aca-8b21-cf71451f4970")
public:
    static const constexpr bool is_unique = true;
    DataStreamSerializedComponents(
            const Id<iscore::Component>& id,
            DataStreamComponents obj,
            QObject* parent);
    virtual ~DataStreamSerializedComponents();


    //! Returns true if all components could be loaded
    bool deserializeRemaining(iscore::Components& comps, QObject* entity);

    bool finished() const
    { return data.empty(); }

    DataStreamComponents data;
};


class ISCORE_LIB_BASE_EXPORT JSONSerializedComponents : public iscore::Component
{
    COMMON_COMPONENT_METADATA("37939615-7165-4bb0-9cdd-e7426153d222")
public:
    static const constexpr bool is_unique = true;
    JSONSerializedComponents(
            const Id<iscore::Component>& id,
            JSONComponents obj,
            QObject* parent);

    virtual ~JSONSerializedComponents();

    //! Returns true if all components could be loaded
    bool deserializeRemaining(iscore::Components& comps, QObject* entity);

    bool finished() const
    { return data.empty(); }

    JSONComponents data;
};


class ISCORE_LIB_BASE_EXPORT SerializableComponent :
        public iscore::Component,
        public iscore::SerializableInterface<iscore::SerializableComponent>
{
public:
    using iscore::Component::Component;

    template<typename Vis>
    SerializableComponent(Vis& vis, QObject* parent):
        iscore::Component{vis, parent}
    {
    }

    virtual InterfaceKey interfaceKey() const = 0;
};

class SerializableComponentFactory :
        public iscore::Interface<iscore::SerializableComponent>
{
    virtual iscore::SerializableComponent* make(
            const Id<iscore::Component>& id,
            const iscore::DocumentContext& ctx,
            QObject* parent) = 0;

    virtual iscore::SerializableComponent* load(
            const VisitorVariant& vis,
            const iscore::DocumentContext& ctx,
            QObject* parent) = 0;
};

class SerializableComponentFactoryList :
        public iscore::InterfaceList<SerializableComponentFactory>
{

};

template <typename System_T>
class GenericSerializableComponent : public iscore::SerializableComponent
{
public:
    template <typename... Args>
    GenericSerializableComponent(System_T& sys, Args&&... args)
        : iscore::SerializableComponent{std::forward<Args>(args)...}
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


struct serializable_tag { };
struct not_serializable_tag { };

template<typename T, typename = void>
struct is_component_serializable
{
  using type = iscore::not_serializable_tag;
};

template<typename T>
struct is_component_serializable<T,
      std::enable_if_t<
        std::is_base_of<iscore::SerializableComponent, T>::value
      >
>
{
  using type = iscore::serializable_tag;
};


template<typename Component_T, typename Fun>
auto deserialize_component(iscore::Components& comps, Fun f)
{
  if(auto datastream_ser = findComponent<DataStreamSerializedComponents>(comps))
  {
    auto& data = datastream_ser->data;
    auto it = data.find(Component_T::static_key());
    if(it != data.end())
    {
      DataStream::Deserializer des{it->second};
      auto res = f(des);
      if(res)
      {
        data.erase(it);
      }
      return res;
    }
  }
  else if(auto json_ser = findComponent<JSONSerializedComponents>(comps))
  {
    auto& data = json_ser->data;
    auto it = data.find(Component_T::static_key());
    if(it != data.end())
    {
      JSONObject::Deserializer des{it->second};
      auto res = f(des);
      if(res)
      {
        data.erase(it);
      }
      return res;
    }
  }
}

ISCORE_LIB_BASE_EXPORT
void deserializeRemainingComponents(iscore::Components& comps, QObject* obj);

}
