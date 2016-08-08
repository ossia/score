#pragma once
#include <OSSIA/LocalTree/BaseCallbackWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIA2iscore.hpp>


namespace Engine
{
namespace LocalTree
{
template<
        typename T,
        typename Object,
        typename PropGet,
        typename PropChanged
>
class QtGetProperty
{
        Object& m_obj;
        PropGet m_get{};
        PropChanged m_changed{};

    public:
        using value_type = T;
        QtGetProperty(
                Object& obj,
                PropGet get,
                PropChanged chgd):
            m_obj{obj},
            m_get{get},
            m_changed{chgd}
        {

        }

        auto get() const
        { return (m_obj.*m_get)(); }

        auto changed() const
        { return (m_obj.*m_changed); }

        auto& object() const
        { return m_obj; }
        auto changed_property() const
        { return m_changed; }
};

template<typename GetProperty>
struct GetPropertyWrapper : public BaseProperty
{
        GetProperty property;
        using converter_t = Engine::ossia_to_iscore::MatchingType<typename GetProperty::value_type>;

        GetPropertyWrapper(
                ossia::net::node_base& param_node,
                ossia::net::address_base& param_addr,
                GetProperty prop,
                QObject* context
                ):
            BaseProperty{param_node, param_addr},
            property{prop}
        {
            QObject::connect(&property.object(), property.changed_property(),
                             context, [=] {

                auto newVal = converter_t::convert(property.get());
                try
                {
                    auto res = Engine::ossia_to_iscore::ToValue(addr.cloneValue());

                    if(newVal != res)
                    {
                        addr.pushValue(Engine::iscore_to_ossia::toOSSIAValue(newVal));
                    }
                }
                catch(...)
                {

                }
            },
            Qt::QueuedConnection);

            addr.setValue(
                        Engine::iscore_to_ossia::toOSSIAValue(
                            converter_t::convert(
                                property.get())));
        }
};

template<typename Property>
auto make_getProperty(
        ossia::net::node_base& node,
        ossia::net::address_base& addr,
        Property prop,
        QObject* context)
{
    return std::make_unique<GetPropertyWrapper<Property>>(node, addr, prop, context);
}

template<typename T, typename Object, typename PropGet, typename PropChanged>
auto add_getProperty(
        ossia::net::node_base& n,
        const std::string& name,
        Object* obj,
        PropGet get,
        PropChanged chgd,
        QObject* context)
{
    constexpr const auto t = Engine::ossia_to_iscore::MatchingType<T>::val;
    auto node = n.createChild(name);
    ISCORE_ASSERT(node);

    auto addr = node->createAddress(t);
    ISCORE_ASSERT(addr);

    addr->setAccessMode(ossia::access_mode::GET);

    return make_getProperty(*node, *addr,
                            QtGetProperty<T, Object, PropGet, PropChanged>{*obj, get, chgd},
                            context);
}
}
}
