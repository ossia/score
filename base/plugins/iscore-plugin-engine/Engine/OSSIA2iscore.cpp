#include <QChar>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <memory>
#include <vector>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Engine
{
namespace ossia_to_iscore
{

static Device::Domain ToDomain(const ossia::net::domain &domain)
{
    Device::Domain d;
    d.min = ToValue(ossia::net::min(domain));
    d.max = ToValue(ossia::net::max(domain));

    ISCORE_TODO;
    // TODO values!!
    // TODO change the i-score domain to use the ossia one.
    return d;
}

Device::AddressSettings ToAddressSettings(const ossia::net::node_base &node)
{
    Device::AddressSettings s;
    s.name = QString::fromStdString(node.getName());

    const auto& addr = node.getAddress();

    if(addr)
    {
        addr->pullValue();

        try {
            s.value = ToValue(addr->cloneValue());
        }
        catch(...)
        {
            s.value = ToValue(addr->getValueType());
        }

        s.ioType = ToIOType(addr->getAccessMode());
        s.clipMode = ToClipMode(addr->getBoundingMode());
        s.repetitionFilter = bool(addr->getRepetitionFilter());
        s.unit = addr->getUnit();
        s.description = QString::fromStdString(addr->getDescription());

        if(auto& domain = addr->getDomain())
            s.domain = ToDomain(domain);
    }
    return s;
}

Device::FullAddressSettings ToFullAddressSettings(const ossia::net::node_base &node)
{
    Device::FullAddressSettings set;
    static_cast<Device::AddressSettingsCommon&>(set) = ToAddressSettings(node);
    set.address = ToAddress(node);
    return set;
}

Device::Node ToDeviceExplorer(const ossia::net::node_base &ossia_node)
{
    Device::Node iscore_node{ToAddressSettings(ossia_node), nullptr};
    iscore_node.reserve(ossia_node.children().size());

    // 2. Recurse on the children
    for(const auto& ossia_child : ossia_node.children())
    {
        auto child_n = ToDeviceExplorer(*ossia_child.get());
        child_n.setParent(&iscore_node);
        iscore_node.push_back(std::move(child_n));
    }

    return iscore_node;
}

Device::IOType ToIOType(ossia::access_mode t)
{
    switch(t)
    {
        case ossia::access_mode::GET:
            return Device::IOType::In;
        case ossia::access_mode::SET:
            return Device::IOType::Out;
        case ossia::access_mode::BI:
            return Device::IOType::InOut;
        default:
            ISCORE_ABORT;
            return Device::IOType::Invalid;
    }
}

Device::ClipMode ToClipMode(ossia::bounding_mode b)
{
    switch(b)
    {
        case ossia::bounding_mode::CLIP:
            return Device::ClipMode::Clip;
        case ossia::bounding_mode::FOLD:
            return Device::ClipMode::Fold;
        case ossia::bounding_mode::FREE:
            return Device::ClipMode::Free;
        case ossia::bounding_mode::WRAP:
            return Device::ClipMode::Wrap;
        case ossia::bounding_mode::LOW:
            return Device::ClipMode::Low;
        case ossia::bounding_mode::HIGH:
            return Device::ClipMode::High;
        default:
            ISCORE_ABORT;
            return static_cast<Device::ClipMode>(-1);
    }
}

State::Address ToAddress(const ossia::net::node_base& node)
{
    State::Address addr;
    const ossia::net::node_base* cur = &node;

    while(auto padre = cur->getParent())
    {
        addr.path.push_front(QString::fromStdString(cur->getName()));
        cur = padre;
    }

    // The last node is the root node "/", which by convention
    // has the same name than the device
    addr.device = QString::fromStdString(cur->getName());
    return addr;
}

State::Value ToValue(ossia::val_type t)
{
    switch(t)
    {
        case ossia::val_type::FLOAT:
            return State::Value::fromValue(float{});
        case ossia::val_type::IMPULSE:
            return State::Value::fromValue(State::impulse_t{});
        case ossia::val_type::INT:
            return State::Value::fromValue(int{});
        case ossia::val_type::BOOL:
            return State::Value::fromValue(bool{});
        case ossia::val_type::CHAR:
            return State::Value::fromValue(QChar{});
        case ossia::val_type::STRING:
            return State::Value::fromValue(QString{});
        case ossia::val_type::TUPLE:
            return State::Value::fromValue(State::tuple_t{});
        case ossia::val_type::VEC2F:
          return State::Value::fromValue(State::vec2f{});
        case ossia::val_type::VEC3F:
            return State::Value::fromValue(State::vec3f{});
        case ossia::val_type::VEC4F:
            return State::Value::fromValue(State::vec4f{});
        case ossia::val_type::DESTINATION:
        case ossia::val_type::BEHAVIOR:
        default:
            return State::Value{};
    }

}

State::Value ToValue(const ossia::value& val)
{
    struct {
            using return_type = State::Value;
            return_type operator()(ossia::Destination) const { return {}; }
            return_type operator()(ossia::Behavior) const { return {}; }
            return_type operator()(ossia::Impulse) const { return State::Value::fromValue(State::impulse_t{}); }
            return_type operator()(ossia::Int v) const { return State::Value::fromValue(v.value); }
            return_type operator()(ossia::Float v) const { return State::Value::fromValue(v.value); }
            return_type operator()(ossia::Bool v) const { return State::Value::fromValue(v.value); }
            return_type operator()(ossia::Char v) const { return State::Value::fromValue(v.value); }
            return_type operator()(const ossia::String& v) const { return State::Value::fromValue(QString::fromStdString(v.value)); }
            return_type operator()(ossia::Vec2f v) const { return State::Value::fromValue(State::vec2f{v.value}); }
            return_type operator()(ossia::Vec3f v) const { return State::Value::fromValue(State::vec3f{v.value}); }
            return_type operator()(ossia::Vec4f v) const { return State::Value::fromValue(State::vec4f{v.value}); }
            return_type operator()(const ossia::Tuple& v) const
            {
                State::tuple_t tuple;

                tuple.reserve(v.value.size());
                for (const auto & e : v.value)
                {
                    tuple.push_back(ToValue(e).val); // TODO REVIEW THIS
                }

                return State::Value::fromValue(std::move(tuple));
            }
    } visitor{};

    if(val.valid())
        return eggs::variants::apply(visitor, val.v);
    return {};
}

}
}
