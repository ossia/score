#include "OSSIA2iscore.hpp"

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
            Q_ASSERT(false);
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
            Q_ASSERT(false);
            return static_cast<iscore::ClipMode>(-1);
    }
}


iscore::Value ToValue(const OSSIA::Value *val)
{
    QVariant v;
    switch(val->getType())
    {
        case OSSIA::Value::Type::IMPULSE:
            break;
        case OSSIA::Value::Type::BOOL:
            v = dynamic_cast<const OSSIA::Bool*>(val)->value;
            break;
        case OSSIA::Value::Type::INT:
            v = dynamic_cast<const OSSIA::Int*>(val)->value;
            break;
        case OSSIA::Value::Type::FLOAT:
            v= dynamic_cast<const OSSIA::Float*>(val)->value;
            break;
        case OSSIA::Value::Type::CHAR:
            v = dynamic_cast<const OSSIA::Char*>(val)->value;
            break;
        case OSSIA::Value::Type::STRING:
            v = QString::fromStdString(dynamic_cast<const OSSIA::String*>(val)->value);
            break;
        case OSSIA::Value::Type::TUPLE:
        {
            QVariantList tuple;
            for (const auto & e : dynamic_cast<const OSSIA::Tuple*>(val)->value)
            {
                tuple.append(ToValue(e).val); // TODO REVIEW THIS
            }

            v = tuple;
            break;
        }
        case OSSIA::Value::Type::GENERIC:
        {
            ISCORE_TODO;
            break;
            /*
            auto generic = dynamic_cast<const OSSIA::Generic*>(val);
            v = QByteArray{generic->start, generic->size};
            break;
            */
        }
        default:
            break;
    }

    return iscore::Value::fromVariant(v);
}


iscore::AddressSettings ToAddressSettings(const OSSIA::Node &node)
{
    iscore::AddressSettings s;
    const auto& addr = node.getAddress();
    s.name = QString::fromStdString(node.getName());

    if(addr)
    {
        addr->updateValue();
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


iscore::Node* ToDeviceExplorer(const OSSIA::Node &node)
{
    iscore::Node* n = new iscore::Node{ToAddressSettings(node)};

    // 2. Recurse on the children
    for(const auto& ossia_child : node.children())
    {
        n->addChild(ToDeviceExplorer(*ossia_child.get()));
    }

    return n;
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
