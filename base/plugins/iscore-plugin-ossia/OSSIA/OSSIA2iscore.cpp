#include <QChar>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <memory>
#include <vector>

#include <ossia/network/domain.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/editor/value/value.hpp>

#include <OSSIA/OSSIA2iscore.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Ossia
{
namespace convert
{

Device::Domain ToDomain(const ossia::net::Domain &domain)
{
    Device::Domain d;
    d.min = ToValue(ossia::net::min(domain));
    d.max = ToValue(ossia::net::max(domain));

    ISCORE_TODO;
    // TODO values!!
    return d;
}

Device::AddressSettings ToAddressSettings(const ossia::net::node &node)
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

        if(auto& domain = addr->getDomain())
            s.domain = ToDomain(domain);
    }
    return s;
}


Device::Node ToDeviceExplorer(const ossia::net::node &ossia_node)
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

Device::IOType ToIOType(ossia::AccessMode t)
{
    switch(t)
    {
        case ossia::AccessMode::GET:
            return Device::IOType::In;
        case ossia::AccessMode::SET:
            return Device::IOType::Out;
        case ossia::AccessMode::BI:
            return Device::IOType::InOut;
        default:
            ISCORE_ABORT;
            return Device::IOType::Invalid;
    }
}

Device::ClipMode ToClipMode(ossia::BoundingMode b)
{
    switch(b)
    {
        case ossia::BoundingMode::CLIP:
            return Device::ClipMode::Clip;
            break;
        case ossia::BoundingMode::FOLD:
            return Device::ClipMode::Fold;
            break;
        case ossia::BoundingMode::FREE:
            return Device::ClipMode::Free;
            break;
        case ossia::BoundingMode::WRAP:
            return Device::ClipMode::Wrap;
            break;
        default:
            ISCORE_ABORT;
            return static_cast<Device::ClipMode>(-1);
    }
}

State::Address ToAddress(const ossia::net::node& node)
{
    State::Address addr;
    const ossia::net::node* cur = &node;

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

State::Value ToValue(ossia::Type t)
{
    switch(t)
    {
        case ossia::Type::FLOAT:
            return State::Value::fromValue(float{});
        case ossia::Type::IMPULSE:
            return State::Value::fromValue(State::impulse_t{});
        case ossia::Type::INT:
            return State::Value::fromValue(int{});
        case ossia::Type::BOOL:
            return State::Value::fromValue(bool{});
        case ossia::Type::CHAR:
            return State::Value::fromValue(QChar{});
        case ossia::Type::STRING:
            return State::Value::fromValue(QString{});
        case ossia::Type::TUPLE:
            return State::Value::fromValue(State::tuple_t{});
        case ossia::Type::DESTINATION:
        case ossia::Type::BEHAVIOR:
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
