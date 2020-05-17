#pragma once
#include <Process/TypeConversion.hpp>

#include <score/tools/std/Invoke.hpp>

#include <ossia/network/base/node.hpp>

#include <LocalTree/BaseCallbackWrapper.hpp>

namespace LocalTree
{
template <typename T, typename SetFun>
struct SetPropertyWrapper final : public BaseCallbackWrapper
{
  SetFun setFun;

  SetPropertyWrapper(ossia::net::parameter_base& param_addr, SetFun prop)
      : BaseCallbackWrapper{param_addr}, setFun{prop}
  {
    callbackIt
        = addr.add_callback([=](const ossia::value& v) { score::invoke([=] { setFun(v); }); });

    // addr.set_value(typename ossia::qt_property_converter<T>::type{});
  }
};

template <typename T, typename Callback>
auto make_setProperty(ossia::net::parameter_base& addr, Callback prop)
{
  return std::make_unique<SetPropertyWrapper<T, Callback>>(addr, prop);
}

template <typename T, typename Callback>
auto add_setProperty(ossia::net::node_base& n, const std::string& name, Callback cb)
{
  constexpr const auto t = ossia::qt_property_converter<T>::val;
  auto node = n.create_child(name);
  SCORE_ASSERT(node);

  auto addr = node->create_parameter(t);
  SCORE_ASSERT(addr);

  addr->set_access(ossia::access_mode::SET);

  return make_setProperty<T>(*addr, cb);
}
}
