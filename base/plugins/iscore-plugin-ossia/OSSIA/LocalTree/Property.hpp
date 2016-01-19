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
                const std::shared_ptr<OSSIA::Node>& param_node,
                const std::shared_ptr<OSSIA::Address>& param_addr,
                Property prop,
                QObject* context
                ):
            BaseCallbackWrapper{param_node, param_addr},
            property{prop}
        {
            callbackIt = addr->addCallback([=] (const OSSIA::Value* v) {
                if(v)
                    property.set(Ossia::convert::ToValue(v));
            });

            QObject::connect(&property.object(), property.changed_property(),
                    context, [=] {
                auto newVal = State::Value::fromValue(property.get());
                if(newVal != Ossia::convert::ToValue(addr->getValue()))
                    addr->pushValue(iscore::convert::toOSSIAValue(newVal));
            },
            Qt::QueuedConnection);

            addr->setValue(iscore::convert::toOSSIAValue(
                                State::Value::fromValue(property.get())));
        }
};

template<typename Property>
auto make_property(
        const std::shared_ptr<OSSIA::Node>& node,
        const std::shared_ptr<OSSIA::Address>& addr,
        Property prop,
        QObject* context)
{
    return std::make_unique<PropertyWrapper<Property>>(node, addr, prop, context);
}

template<typename T, typename Object, typename PropGet, typename PropSet, typename PropChanged>
auto add_property(
        OSSIA::Node& n,
        const std::string& name,
        Object* obj,
        PropGet get,
        PropSet set,
        PropChanged chgd,
        QObject* context)
{
    constexpr const auto t = Ossia::convert::MatchingType<T>::val;
    std::shared_ptr<OSSIA::Node> node = *n.emplace(
                                            n.children().end(),
                                            name,
                                            t,
                                            OSSIA::AccessMode::BI);

    return make_property(node,
                         node->getAddress(),
                         QtProperty<T, Object, PropGet, PropSet, PropChanged>{*obj, get, set, chgd},
                         context);
}
}
}
