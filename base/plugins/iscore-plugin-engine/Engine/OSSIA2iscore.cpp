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
Device::AddressSettings ToAddressSettings(const ossia::net::node_base &node)
{
    Device::AddressSettings s;
    s.name = QString::fromStdString(node.getName());

    const auto& addr = node.getAddress();

    if(addr)
    {
        auto value_future = addr->pullValueAsync();

        s.ioType = ToIOType(addr->getAccessMode());
        s.clipMode = ToClipMode(addr->getBoundingMode());
        s.repetitionFilter = bool(addr->getRepetitionFilter());
        s.unit = addr->getUnit();
        s.description = QString::fromStdString(addr->getDescription());
        s.domain = addr->getDomain();

        if(value_future.valid())
          value_future.wait_for(std::chrono::milliseconds(25));

        try {
            s.value = State::fromOSSIAValue(addr->cloneValue());
        }
        catch(...)
        {
            s.value = ToValue(addr->getValueType());
        }
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
        default:
            return State::Value{};
    }

}

}
}
