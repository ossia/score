#pragma once
#include <OSSIA/LocalTree/BaseCallbackWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIA2iscore.hpp>

namespace Ossia
{
namespace LocalTree
{
template<typename T, typename SetFun>
struct SetPropertyWrapper : public BaseCallbackWrapper
{
        SetFun setFun;

        SetPropertyWrapper(
                const std::shared_ptr<OSSIA::Node>& param_node,
                const std::shared_ptr<OSSIA::Address>& param_addr,
                SetFun prop
                ):
            BaseCallbackWrapper{param_node, param_addr},
            setFun{prop}
        {
            callbackIt = addr->addCallback([=] (const OSSIA::Value* v) {
                setFun(Ossia::convert::ToValue(v));
            });

            addr->setValue(new typename Ossia::convert::MatchingType<T>::type);
        }
};

template<typename T, typename Callback>
auto make_setProperty(
        const std::shared_ptr<OSSIA::Node>& node,
        const std::shared_ptr<OSSIA::Address>& addr,
        Callback prop)
{
    return std::make_unique<SetPropertyWrapper<T, Callback>>(node, addr, prop);
}

template<typename T, typename Callback>
auto add_setProperty(OSSIA::Node& n, const std::string& name, Callback cb)
{
    constexpr const auto t = Ossia::convert::MatchingType<T>::val;
    std::shared_ptr<OSSIA::Node> node = *n.emplaceAndNotify(
                                            n.children().end(),
                                            name,
                                            t,
                                            OSSIA::AccessMode::SET);

    return make_setProperty<T>(node, node->getAddress(), cb);
}
}
}
