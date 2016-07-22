#include <QChar>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <memory>
#include <vector>

#include "Editor/Domain.h"
#include <Editor/Value/Value.h>
#include "Network/Address.h"
#include "Network/Device.h"
#include "Network/Node.h"
#include <OSSIA/OSSIA2iscore.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Ossia
{
namespace convert
{


Device::IOType ToIOType(OSSIA::AccessMode t)
{
    switch(t)
    {
        case OSSIA::AccessMode::GET:
            return Device::IOType::In;
        case OSSIA::AccessMode::SET:
            return Device::IOType::Out;
        case OSSIA::AccessMode::BI:
            return Device::IOType::InOut;
        default:
            ISCORE_ABORT;
            return Device::IOType::Invalid;
    }
}


Device::ClipMode ToClipMode(OSSIA::BoundingMode b)
{
    switch(b)
    {
        case OSSIA::BoundingMode::CLIP:
            return Device::ClipMode::Clip;
            break;
        case OSSIA::BoundingMode::FOLD:
            return Device::ClipMode::Fold;
            break;
        case OSSIA::BoundingMode::FREE:
            return Device::ClipMode::Free;
            break;
        case OSSIA::BoundingMode::WRAP:
            return Device::ClipMode::Wrap;
            break;
        default:
            ISCORE_ABORT;
            return static_cast<Device::ClipMode>(-1);
    }
}

State::Address ToAddress(const OSSIA::Node& node)
{
    State::Address addr;
    const OSSIA::Node* cur = &node;

    while(!dynamic_cast<const OSSIA::Device*>(cur))
    {
        addr.path.push_front(QString::fromStdString(cur->getName()));
        cur = cur->getParent().get();
        ISCORE_ASSERT(cur);
    }

    ISCORE_ASSERT(dynamic_cast<const OSSIA::Device*>(cur));
    addr.device = QString::fromStdString(cur->getName());
    return addr;
}

Device::AddressSettings ToAddressSettings(const OSSIA::Node &node)
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

        /* Debug code
        else
        {
            QStringList total;
            auto nptr = &node;
            while(nptr)
            {
                total += QString::fromStdString(nptr->getName());
                if(nptr->getParent())
                {
                    nptr = nptr->getParent().get();
                }
                else
                    break;
            }
            std::reverse(total.begin(), total.end());
            qDebug() << total.join("/");
        }
        */

        s.ioType = ToIOType(addr->getAccessMode());
        s.clipMode = ToClipMode(addr->getBoundingMode());
        s.repetitionFilter = addr->getRepetitionFilter();

        if(auto& domain = addr->getDomain())
            s.domain = ToDomain(*domain);
    }
    return s;
}


Device::Node ToDeviceExplorer(const OSSIA::Node &ossia_node)
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


Device::Domain ToDomain(OSSIA::Domain &domain)
{
    Device::Domain d;
    d.min = ToValue(domain.getMin());
    d.max = ToValue(domain.getMax());

    for(const auto& val : domain.getValues())
    {
        d.values.append(ToValue(val));
    }

    return d;
}

State::Value ToValue(OSSIA::Type t)
{
    switch(t)
    {
        case OSSIA::Type::FLOAT:
            return State::Value::fromValue(float{});
        case OSSIA::Type::IMPULSE:
            return State::Value::fromValue(State::impulse_t{});
        case OSSIA::Type::INT:
            return State::Value::fromValue(int{});
        case OSSIA::Type::BOOL:
            return State::Value::fromValue(bool{});
        case OSSIA::Type::CHAR:
            return State::Value::fromValue(QChar{});
        case OSSIA::Type::STRING:
            return State::Value::fromValue(QString{});
        case OSSIA::Type::TUPLE:
            return State::Value::fromValue(State::tuple_t{});
        case OSSIA::Type::DESTINATION:
        case OSSIA::Type::BEHAVIOR:
        default:
            return State::Value{};
    }

}

State::Value ToValue(const OSSIA::SafeValue& val)
{
    struct {
            using return_type = State::Value;
            return_type operator()(OSSIA::Destination) const { return {}; }
            return_type operator()(OSSIA::Behavior) const { return {}; }
            return_type operator()(OSSIA::Impulse) const { return State::Value::fromValue(State::impulse_t{}); }
            return_type operator()(OSSIA::Int v) const { return State::Value::fromValue(v.value); }
            return_type operator()(OSSIA::Float v) const { return State::Value::fromValue(v.value); }
            return_type operator()(OSSIA::Bool v) const { return State::Value::fromValue(v.value); }
            return_type operator()(OSSIA::Char v) const { return State::Value::fromValue(v.value); }
            return_type operator()(const OSSIA::String& v) const { return State::Value::fromValue(QString::fromStdString(v.value)); }
            return_type operator()(OSSIA::Vec2f v) const { return State::Value::fromValue(State::vec2f{v.value}); }
            return_type operator()(OSSIA::Vec3f v) const { return State::Value::fromValue(State::vec3f{v.value}); }
            return_type operator()(OSSIA::Vec4f v) const { return State::Value::fromValue(State::vec4f{v.value}); }
            return_type operator()(const OSSIA::Tuple& v) const
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
