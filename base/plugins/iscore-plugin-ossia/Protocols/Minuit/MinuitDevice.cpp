#include "MinuitDevice.hpp"
#include <API/Headers/Network/Node.h>
#include <API/Headers/Network/AddressValue.h>
#include <Plugin/Common/AddressSettings/AddressSpecificSettings/AddressIntSettings.hpp>
#include <Plugin/Common/AddressSettings/AddressSpecificSettings/AddressFloatSettings.hpp>
MinuitDevice::MinuitDevice(const DeviceSettings &settings):
    OSSIADevice{settings},
    m_minuitSettings{[&] () {
    auto stgs = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
    return OSSIA::Minuit{stgs.host.toStdString(),
        stgs.inPort,
        stgs.outPort};
}()}

{
    using namespace OSSIA;

    m_dev = Device::create(m_minuitSettings, settings.name.toStdString());

    Q_ASSERT(m_dev);
}

bool MinuitDevice::canRefresh() const
{
    return true;
}

// Utility functions to convert from one node to another.
namespace
{
    IOType accessModeToIOType(OSSIA::Address::AccessMode t)
    {
        switch(t)
        {
            case OSSIA::Address::AccessMode::GET:
                return IOType::In;
            case OSSIA::Address::AccessMode::SET:
                return IOType::Out;
            case OSSIA::Address::AccessMode::BI:
                return IOType::InOut;
            default:
                Q_ASSERT(false);
                return IOType::Invalid;
        }
    }

    ClipMode OSSIABoudingModeToClipMode(OSSIA::Address::BoundingMode b)
    {
        switch(b)
        {
            case OSSIA::Address::BoundingMode::CLIP:
                return ClipMode::Clip;
                break;
            case OSSIA::Address::BoundingMode::FOLD:
                return ClipMode::Fold;
                break;
            case OSSIA::Address::BoundingMode::FREE:
                return ClipMode::Free;
                break;
            case OSSIA::Address::BoundingMode::WRAP:
                return ClipMode::Wrap;
                break;
            default:
                Q_ASSERT(false);
                return static_cast<ClipMode>(-1);
        }
    }

    QVariant OSSIAValueToVariant(const OSSIA::AddressValue* val)
    {
        QVariant v;
        switch(val->getType())
        {
            case OSSIA::AddressValue::Type::NONE:
                break;
            case OSSIA::AddressValue::Type::BOOL:
                v = dynamic_cast<const OSSIA::Bool*>(val)->value;
                break;
            case OSSIA::AddressValue::Type::INT:
                v = dynamic_cast<const OSSIA::Int*>(val)->value;
                break;
            case OSSIA::AddressValue::Type::FLOAT:
                v= dynamic_cast<const OSSIA::Float*>(val)->value;
                break;
            case OSSIA::AddressValue::Type::CHAR:
                v = dynamic_cast<const OSSIA::Char*>(val)->value;
                break;
            case OSSIA::AddressValue::Type::STRING:
                v = QString::fromStdString(dynamic_cast<const OSSIA::String*>(val)->value);
                break;
            case OSSIA::AddressValue::Type::TUPLE:
            {
                QVariantList tuple;
                for (const auto & e : dynamic_cast<const OSSIA::Tuple*>(val)->value)
                {
                    tuple.append(OSSIAValueToVariant(e));
                }

                v = tuple;
                break;
            }
            case OSSIA::AddressValue::Type::GENERIC:
                qDebug() << Q_FUNC_INFO << "todo";
                break;
            default:
                break;
        }

        return v;
    }

    AddressSettings extractAddressSettings(const OSSIA::Node& node)
    {
        AddressSettings s;
        const auto& addr = node.getAddress();
        s.name = QString::fromStdString(node.getName());

        if(addr)
        {
            s.value = OSSIAValueToVariant(addr->getValue());
            s.ioType = accessModeToIOType(addr->getAccessMode());
            s.clipMode = OSSIABoudingModeToClipMode(addr->getBoundingMode());
            s.repetitionFilter = addr->getRepetitionFilter();

            // TODO priority

        }
        return s;
    }

    Node* OssiaToDeviceExplorer(const OSSIA::Node& node)
    {
        Node* n = new Node{extractAddressSettings(node)};

        // 2. Recurse on the children
        for(const auto& ossia_child : node.children())
        {
            n->addChild(OssiaToDeviceExplorer(*ossia_child.get()));
        }

        return n;
    }
}

Node MinuitDevice::refresh()
{
    Node device_node;

    try {
    if(m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        device_node.setDeviceSettings(settings());
        device_node.setAddressSettings(extractAddressSettings(*m_dev.get()));

        // Recurse on the children
        for(const auto& node : m_dev->children())
        {
            device_node.addChild(OssiaToDeviceExplorer(*node.get()));
        }
    }
    }
    catch(std::runtime_error& e)
    {
        qDebug() << "Couldn't load the device:" << e.what();
    }

    device_node.deviceSettings().name = settings().name;

    return device_node;
}
