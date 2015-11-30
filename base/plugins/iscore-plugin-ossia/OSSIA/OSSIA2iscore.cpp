#include <QChar>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <memory>
#include <vector>

#include "Editor/Domain.h"
#include "Editor/Value.h"
#include "Network/Address.h"
#include "Network/Node.h"
#include <OSSIA/OSSIA2iscore.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace OSSIA
{
namespace convert
{
iscore::IOType ToIOType(OSSIA::Address::AccessMode t)
{
    switch(t)
    {
        case OSSIA::Address::AccessMode::GET:
            return iscore::IOType::In;
        case OSSIA::Address::AccessMode::SET:
            return iscore::IOType::Out;
        case OSSIA::Address::AccessMode::BI:
            return iscore::IOType::InOut;
        default:
            ISCORE_ABORT;
            return iscore::IOType::Invalid;
    }
}


iscore::ClipMode ToClipMode(OSSIA::Address::BoundingMode b)
{
    switch(b)
    {
        case OSSIA::Address::BoundingMode::CLIP:
            return iscore::ClipMode::Clip;
            break;
        case OSSIA::Address::BoundingMode::FOLD:
            return iscore::ClipMode::Fold;
            break;
        case OSSIA::Address::BoundingMode::FREE:
            return iscore::ClipMode::Free;
            break;
        case OSSIA::Address::BoundingMode::WRAP:
            return iscore::ClipMode::Wrap;
            break;
        default:
            ISCORE_ABORT;
            return static_cast<iscore::ClipMode>(-1);
    }
}

iscore::Value ToValue(const OSSIA::Value *val)
{
    if(!val)
        return {};

    // TODO this should be a dynamic_cast every time
    // for safety ?
    switch(val->getType())
    {
        case OSSIA::Value::Type::IMPULSE:
            return iscore::Value::fromValue(iscore::impulse_t{});
        case OSSIA::Value::Type::BOOL:
            return iscore::Value::fromValue(safe_cast<const OSSIA::Bool*>(val)->value);
        case OSSIA::Value::Type::INT:
            return iscore::Value::fromValue(safe_cast<const OSSIA::Int*>(val)->value);
        case OSSIA::Value::Type::FLOAT:
            return iscore::Value::fromValue(safe_cast<const OSSIA::Float*>(val)->value);
        case OSSIA::Value::Type::CHAR:
            return iscore::Value::fromValue(QChar::fromLatin1(safe_cast<const OSSIA::Char*>(val)->value));
        case OSSIA::Value::Type::STRING:
            return iscore::Value::fromValue(QString::fromStdString(safe_cast<const OSSIA::String*>(val)->value));
        case OSSIA::Value::Type::TUPLE:
        {
            auto ossia_tuple = safe_cast<const OSSIA::Tuple*>(val);
            iscore::tuple_t tuple;
            tuple.reserve(ossia_tuple->value.size());
            for (const auto & e : ossia_tuple->value)
            {
                tuple.push_back(ToValue(e).val); // TODO REVIEW THIS
            }

            return iscore::Value::fromValue(tuple);
        }
        case OSSIA::Value::Type::GENERIC:
        {
            ISCORE_TODO;
            return {};
            /*
            auto generic = dynamic_cast<const OSSIA::Generic*>(val);
            v = QByteArray{generic->start, generic->size};
            break;
            */
        }
        case OSSIA::Value::Type::DESTINATION:
        case OSSIA::Value::Type::BEHAVIOR:
        default:
            return {};
    }
}


iscore::AddressSettings ToAddressSettings(const OSSIA::Node &node)
{
    iscore::AddressSettings s;
    s.name = QString::fromStdString(node.getName());

    const auto& addr = node.getAddress();

    if(addr)
    {
        addr->pullValue();
        if(auto val = addr->getValue())
            s.value = ToValue(val);

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


iscore::Node ToDeviceExplorer(const OSSIA::Node &ossia_node)
{
    iscore::Node iscore_node{ToAddressSettings(ossia_node), nullptr};
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


iscore::Domain ToDomain(OSSIA::Domain &domain)
{
    iscore::Domain d;
    d.min = ToValue(domain.getMin());
    d.max = ToValue(domain.getMax());

    for(const auto& val : domain.getValues())
    {
        d.values.append(ToValue(val));
    }

    return d;
}
}
}
