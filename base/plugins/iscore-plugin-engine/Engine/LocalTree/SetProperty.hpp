#pragma once
#include <Engine/LocalTree/BaseCallbackWrapper.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/OSSIA2iscore.hpp>

namespace Engine
{
namespace LocalTree
{
template<typename T, typename SetFun>
struct SetPropertyWrapper : public BaseCallbackWrapper
{
        SetFun setFun;

        SetPropertyWrapper(
                ossia::net::node_base& param_node,
                ossia::net::address_base& param_addr,
                SetFun prop
                ):
            BaseCallbackWrapper{param_node, param_addr},
            setFun{prop}
        {
            callbackIt = addr.add_callback([=] (const ossia::value& v) {
                setFun(Engine::ossia_to_iscore::ToValue(v));
            });

            addr.setValue(typename Engine::ossia_to_iscore::MatchingType<T>::type{});
        }
};

template<typename T, typename Callback>
auto make_setProperty(
        ossia::net::node_base& node,
        ossia::net::address_base& addr,
        Callback prop)
{
    return std::make_unique<SetPropertyWrapper<T, Callback>>(node, addr, prop);
}

template<typename T, typename Callback>
auto add_setProperty(ossia::net::node_base& n, const std::string& name, Callback cb)
{
    constexpr const auto t = Engine::ossia_to_iscore::MatchingType<T>::val;
    auto node = n.createChild(name);
    ISCORE_ASSERT(node);

    auto addr = node->createAddress(t);
    ISCORE_ASSERT(addr);

    addr->setAccessMode(ossia::access_mode::SET);

    return make_setProperty<T>(*node, *addr, cb);
}
}
}
