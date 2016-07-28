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
        using converter_t = Ossia::convert::MatchingType<typename GetProperty::value_type>;

        GetPropertyWrapper(
                OSSIA::net::Node& param_node,
                OSSIA::net::Address& param_addr,
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
                    auto res = Ossia::convert::ToValue(addr.cloneValue());

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

            addr.setValue(
                        iscore::convert::toOSSIAValue(
                            converter_t::convert(
                                property.get())));
        }
};

template<typename Property>
auto make_getProperty(
        OSSIA::net::Node& node,
        OSSIA::net::Address& addr,
        Property prop,
        QObject* context)
{
    return std::make_unique<GetPropertyWrapper<Property>>(node, addr, prop, context);
}

template<typename T, typename Object, typename PropGet, typename PropChanged>
auto add_getProperty(
        OSSIA::net::Node& n,
        const std::string& name,
        Object* obj,
        PropGet get,
        PropChanged chgd,
        QObject* context)
{
    constexpr const auto t = Ossia::convert::MatchingType<T>::val;
    auto node = n.createChild(name);
    ISCORE_ASSERT(node);

    auto addr = node->createAddress(t);
    ISCORE_ASSERT(addr);

    addr->setAccessMode(OSSIA::AccessMode::GET);

    return make_getProperty(*node, *addr,
                            QtGetProperty<T, Object, PropGet, PropChanged>{*obj, get, chgd},
                            context);
}
}
}
