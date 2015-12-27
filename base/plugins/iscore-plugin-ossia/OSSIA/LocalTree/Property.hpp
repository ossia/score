#pragma once
#include <OSSIA/LocalTree/BaseCallbackWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIA2iscore.hpp>

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
        auto set(const iscore::Value& newval) const
        { return (m_obj.*m_set)(iscore::convert::value<T>(newval)); }

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
                const std::shared_ptr<OSSIA::Node>& node,
                const std::shared_ptr<OSSIA::Address>& addr,
                Property prop,
                QObject* context
                ):
            BaseCallbackWrapper{node, addr},
            property{prop}
        {
            m_callbackIt = addr->addCallback([=] (const OSSIA::Value* v) {
                if(v)
                    property.set(OSSIA::convert::ToValue(v));
            });

            QObject::connect(&property.object(), property.changed_property(),
                    context, [=] {
                auto newVal = iscore::Value::fromValue(property.get());
                if(newVal != OSSIA::convert::ToValue(addr->getValue()))
                    addr->pushValue(iscore::convert::toOSSIAValue(newVal));
            },
            Qt::QueuedConnection);

            addr->setValue(iscore::convert::toOSSIAValue(
                                iscore::Value::fromValue(property.get())));
        }
};

template<typename Property>
auto make_property(
        const std::shared_ptr<OSSIA::Node>& node,
        const std::shared_ptr<OSSIA::Address>& addr,
        Property prop,
        QObject* context)
{
    return new PropertyWrapper<Property>{node, addr, prop, context};
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
    std::shared_ptr<OSSIA::Node> node = *n.emplaceAndNotify(n.children().end(), name);
    auto addr = node->createAddress(OSSIA::convert::MatchingType<T>::val);
    addr->setAccessMode(OSSIA::Address::AccessMode::BI);

    return make_property(node,
                         addr,
                         QtProperty<T, Object, PropGet, PropSet, PropChanged>{*obj, get, set, chgd},
                         context);
}
