#pragma once
#include <Engine/LocalTree/BaseCallbackWrapper.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Engine/score2OSSIA.hpp>

namespace Engine
{
namespace LocalTree
{
template <typename T, typename Object, typename PropGet, typename PropChanged>
class QtGetProperty
{
  Object& m_obj;
  PropGet m_get{};
  PropChanged m_changed{};

public:
  using value_type = T;
  QtGetProperty(Object& obj, PropGet get, PropChanged chgd)
      : m_obj{obj}, m_get{get}, m_changed{chgd}
  {
  }

  auto get() const
  {
    return (m_obj.*m_get)();
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

template <typename GetProperty>
struct GetPropertyWrapper final : public BaseProperty
{
  GetProperty property;
  using converter_t
      = Engine::ossia_to_score::MatchingType<typename GetProperty::value_type>;

  GetPropertyWrapper(
      ossia::net::node_base& param_node,
      ossia::net::parameter_base& param_addr,
      GetProperty prop,
      QObject* context)
      : BaseProperty{param_node, param_addr}, property{prop}
  {
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
auto make_getProperty(
    ossia::net::node_base& node,
    ossia::net::parameter_base& addr,
    Property prop,
    QObject* context)
{
  return std::make_unique<GetPropertyWrapper<Property>>(
      node, addr, prop, context);
}

template <typename T, typename Object, typename PropGet, typename PropChanged>
auto add_getProperty(
    ossia::net::node_base& n,
    const std::string& name,
    Object* obj,
    PropGet get,
    PropChanged chgd,
    QObject* context)
{
  constexpr const auto t = Engine::ossia_to_score::MatchingType<T>::val;
  auto node = n.create_child(name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::GET);

  return make_getProperty(
      *node, *addr,
      QtGetProperty<T, Object, PropGet, PropChanged>{*obj, get, chgd},
      context);
}
}
}
