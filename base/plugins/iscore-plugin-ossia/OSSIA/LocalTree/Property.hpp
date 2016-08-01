#pragma once
#include <OSSIA/LocalTree/BaseCallbackWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIA2iscore.hpp>

namespace Ossia
{
namespace LocalTree
{
template<
        typename T,
        typename Object,
        typename PropGet,
        typename PropSet,
        typename PropChanged
>
class QtProperty
{
        Object& m_obj;
        PropGet m_get{};
        PropSet m_set{};
        PropChanged m_changed{};

    public:
        QtProperty(
                Object& obj,
                PropGet get,
                PropSet set,
                PropChanged chgd):
            m_obj{obj},
            m_get{get},
            m_set{set},
            m_changed{chgd}
        {

        }

        auto get() const
        { return (m_obj.*m_get)(); }

        auto set(const T& newval) const
        { return (m_obj.*m_set)(newval); }
        auto set(const State::Value& newval) const
        { return (m_obj.*m_set)(State::convert::value<T>(newval)); }

        auto changed() const
        { return (m_obj.*m_changed); }

        auto& object() const
        { return m_obj; }
        auto changed_property() const
        { return m_changed; }
};

template<typename Property>
struct PropertyWrapper : public BaseCallbackWrapper
{
        Property property;

        PropertyWrapper(
                ossia::net::node_base& param_node,
                ossia::net::address_base& param_addr,
                Property prop,
                QObject* context
                ):
            BaseCallbackWrapper{param_node, param_addr},
            property{prop}
        {
            callbackIt = addr.add_callback([=] (const ossia::value& v) {
                    property.set(Ossia::convert::ToValue(v));
            });

            QObject::connect(&property.object(), property.changed_property(),
                    context, [=] {
                auto newVal = State::Value::fromValue(property.get());
                try
                {
                    auto res = Ossia::convert::ToValue( addr.cloneValue());

                    if(newVal != res)
                    {
                        addr.pushValue(iscore::convert::toOSSIAValue(newVal));
                    }
                }
                catch(...)
                {

                }
            },
            Qt::QueuedConnection);

            {
                addr.setValue(
                            iscore::convert::toOSSIAValue(
                                State::Value::fromValue(
                                    property.get())));
            }
        }
};

template<typename Property>
auto make_property(
        ossia::net::node_base& node,
        ossia::net::address_base& addr,
        Property prop,
        QObject* context)
{
    return std::make_unique<PropertyWrapper<Property>>(node, addr, prop, context);
}

template<typename T, typename Object, typename PropGet, typename PropSet, typename PropChanged>
auto add_property(
        ossia::net::node_base& n,
        const std::string& name,
        Object* obj,
        PropGet get,
        PropSet set,
        PropChanged chgd,
        QObject* context)
{
    constexpr const auto t = Ossia::convert::MatchingType<T>::val;
    auto node = n.createChild(name);
    ISCORE_ASSERT(node);

    auto addr = node->createAddress(t);
    ISCORE_ASSERT(addr);

    addr->setAccessMode(ossia::access_mode::BI);

    return make_property(*node,
                         *addr,
                         QtProperty<T, Object, PropGet, PropSet, PropChanged>{*obj, get, set, chgd},
                         context);
}
}
}
