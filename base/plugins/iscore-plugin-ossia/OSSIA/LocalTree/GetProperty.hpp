#pragma once
#include <OSSIA/LocalTree/BaseCallbackWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIA2iscore.hpp>

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
        using converter_t = OSSIA::convert::MatchingType<typename GetProperty::value_type>;

        GetPropertyWrapper(
                const std::shared_ptr<OSSIA::Node>& node,
                const std::shared_ptr<OSSIA::Address>& addr,
                GetProperty prop,
                QObject* context
                ):
            BaseProperty{node, addr},
            property{prop}
        {
            QObject::connect(&property.object(), property.changed_property(),
                             context, [=] {
                auto newVal = converter_t::convert(property.get());
                if(newVal != OSSIA::convert::ToValue(addr->getValue()))
                    addr->pushValue(iscore::convert::toOSSIAValue(newVal));
            },
            Qt::QueuedConnection);

            addr->setValue(iscore::convert::toOSSIAValue(converter_t::convert(property.get())));
        }
};

template<typename Property>
auto make_getProperty(
        const std::shared_ptr<OSSIA::Node>& node,
        const std::shared_ptr<OSSIA::Address>& addr,
        Property prop,
        QObject* context)
{
    return new GetPropertyWrapper<Property>{node, addr, prop, context};
}

template<typename T, typename Object, typename PropGet, typename PropChanged>
auto add_getProperty(
        OSSIA::Node& n,
        const std::string& name,
        Object* obj,
        PropGet get,
        PropChanged chgd,
        QObject* context)
{
    std::shared_ptr<OSSIA::Node> node = *n.emplaceAndNotify(n.children().end(), name);
    auto addr = node->createAddress(OSSIA::convert::MatchingType<T>::val);
    addr->setAccessMode(OSSIA::Address::AccessMode::GET);

    return make_getProperty(node, addr, QtGetProperty<T, Object, PropGet, PropChanged>{*obj, get, chgd}, context);
}
