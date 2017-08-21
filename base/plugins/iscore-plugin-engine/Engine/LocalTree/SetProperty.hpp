#pragma once
#include <Engine/LocalTree/BaseCallbackWrapper.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>

namespace Engine
{
namespace LocalTree
{
template <typename T, typename SetFun>
struct SetPropertyWrapper final : public BaseCallbackWrapper
{
  SetFun setFun;

  SetPropertyWrapper(
      ossia::net::node_base& param_node,
      ossia::net::parameter_base& param_addr,
      SetFun prop)
      : BaseCallbackWrapper{param_node, param_addr}, setFun{prop}
  {
    callbackIt = addr.add_callback(
        [=](const ossia::value& v) { setFun(v); });

    addr.set_value(typename Engine::ossia_to_iscore::MatchingType<T>::type{});
  }
};

template <typename T, typename Callback>
auto make_setProperty(
    ossia::net::node_base& node, ossia::net::parameter_base& addr, Callback prop)
{
  return std::make_unique<SetPropertyWrapper<T, Callback>>(node, addr, prop);
}

template <typename T, typename Callback>
auto add_setProperty(
    ossia::net::node_base& n, const std::string& name, Callback cb)
{
  constexpr const auto t = Engine::ossia_to_iscore::MatchingType<T>::val;
  auto node = n.create_child(name);
  ISCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  ISCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::SET);

  return make_setProperty<T>(*node, *addr, cb);
}
}
}
