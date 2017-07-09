#pragma once
#include <Engine/LocalTree/BaseCallbackWrapper.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <State/Value.hpp>

namespace Engine
{
namespace LocalTree
{
template <
    typename T, typename Object, typename PropGet, typename PropSet,
    typename PropChanged>
class QtProperty
{
  Object& m_obj;
  PropGet m_get{};
  PropSet m_set{};
  PropChanged m_changed{};

public:
  using value_type = T;
  QtProperty(Object& obj, PropGet get, PropSet set, PropChanged chgd)
      : m_obj{obj}, m_get{get}, m_set{set}, m_changed{chgd}
  {
  }

  auto get() const
  {
    return (m_obj.*m_get)();
  }

  auto set(const T& newval) const
  {
    return (m_obj.*m_set)(newval);
  }
  auto set(const ::ossia::value& newval) const
  {
    return (m_obj.*m_set)(::State::convert::value<T>(newval));
  }

  auto changed() const
  {
    return (m_obj.*m_changed);
  }

  auto& object() const
  {
    return m_obj;
  }
  auto changed_property() const
  {
    return m_changed;
  }
};

template<typename T>
using MatchingType_T =
  Engine::ossia_to_iscore::MatchingType<
    std::remove_const_t<
      std::remove_reference_t<T>
    >
  >;

template <typename Property>
struct PropertyWrapper final : public BaseCallbackWrapper
{
  Property property;
  using converter_t = Engine::ossia_to_iscore::MatchingType<
      typename Property::value_type>;
  PropertyWrapper(
      ossia::net::node_base& param_node,
      ossia::net::address_base& param_addr,
      Property prop,
      QObject* context)
      : BaseCallbackWrapper{param_node, param_addr}, property{prop}
  {
    callbackIt = addr.add_callback([=](const ossia::value& v) {
      property.set(v);
    });

    QObject::connect(
        &property.object(), property.changed_property(), context,
        [=] {
          auto newVal = converter_t::convert(property.get());
          try
          {
            auto res = addr.value();

            if (newVal != res)
            {
              addr.push_value(newVal);
            }
          }
          catch (...)
          {
          }
        },
        Qt::QueuedConnection);

    addr.set_value(converter_t::convert(property.get()));
  }
};

template <typename Property>
auto make_property(
    ossia::net::node_base& node,
    ossia::net::address_base& addr,
    Property prop,
    QObject* context)
{
  return std::make_unique<PropertyWrapper<Property>>(
      node, addr, prop, context);
}

template <
    typename T, typename Object, typename PropGet, typename PropSet,
    typename PropChanged>
auto add_property(
    ossia::net::node_base& n,
    const std::string& name,
    Object* obj,
    PropGet get,
    PropSet set,
    PropChanged chgd,
    QObject* context)
{
  constexpr const auto t = Engine::ossia_to_iscore::MatchingType<T>::val;
  auto node = n.create_child(name);
  ISCORE_ASSERT(node);

  auto addr = node->create_address(t);
  ISCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::BI);

  return make_property(
      *node,
      *addr,
      QtProperty<T, Object, PropGet, PropSet, PropChanged>{*obj, get, set,
                                                           chgd},
      context);
}
}
}
