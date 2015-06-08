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
}

bool MinuitDevice::canRefresh() const
{
    return true;
}

// Utility functions to convert from one node to another.
namespace
{
    QString valueTypeToString(OSSIA::AddressValue::Type t)
    {
        switch(t)
        {
            case OSSIA::AddressValue::Type::NONE:
                return "None";
            case OSSIA::AddressValue::Type::BOOL:
                return "Bool";
            case OSSIA::AddressValue::Type::INT:
                return "Int";
            case OSSIA::AddressValue::Type::FLOAT:
                return "Float";
            case OSSIA::AddressValue::Type::CHAR:
                return "Char";
            case OSSIA::AddressValue::Type::STRING:
                return "String";
            case OSSIA::AddressValue::Type::TUPLE:
                return "Tuple";
            case OSSIA::AddressValue::Type::GENERIC:
                return "Generic";
            default:
                Q_ASSERT(false);
                return "";
        }
    }

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

    AddressSettings extractAddressSettings(const OSSIA::Node& node)
    {
        AddressSettings s;
        const auto& addr = node.getAddress();
        s.name = QString::fromStdString(node.getName());

        if(addr)
        {
            s.valueType = valueTypeToString(addr->getValueType());
            s.ioType = accessModeToIOType(addr->getAccessMode());
            // TODO priority

            switch(addr->getValueType())
            {
                case OSSIA::AddressValue::Type::NONE:
                    break;
                case OSSIA::AddressValue::Type::BOOL:
                    s.value = dynamic_cast<OSSIA::Bool*>(addr->getValue())->value;
                    break;
                case OSSIA::AddressValue::Type::INT:
                    s.value = dynamic_cast<OSSIA::Int*>(addr->getValue())->value;
                    break;
                case OSSIA::AddressValue::Type::FLOAT:
                    s.value = dynamic_cast<OSSIA::Float*>(addr->getValue())->value;
                    break;
                case OSSIA::AddressValue::Type::CHAR:
                    qDebug() << Q_FUNC_INFO << "todo";
                    break;
                case OSSIA::AddressValue::Type::STRING:
                    s.value = QString::fromStdString(dynamic_cast<OSSIA::String*>(addr->getValue())->value);
                    break;
                case OSSIA::AddressValue::Type::TUPLE:
                    qDebug() << Q_FUNC_INFO << "todo";
                    break;
                case OSSIA::AddressValue::Type::GENERIC:
                    qDebug() << Q_FUNC_INFO << "todo";
                    break;
                default:
                    break;
            }
        }
        return s;
    }

    Node* OssiaToDeviceExplorer(const OSSIA::Node& node)
    {
        Node* n = new Node;

        // 1. Set the parameters
        n->setAddressSettings(extractAddressSettings(node));

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

    Node device;

    if(m_dev->updateNamespace())
    {
        // Make a device explorer node from the current state of the device.
        // First make the node corresponding to the root node.

        device.setDeviceSettings(settings());
        device.setAddressSettings(extractAddressSettings(*m_dev.get()));

        // Recurse on the children
        for(const auto& node : m_dev->children())
        {
            device.addChild(OssiaToDeviceExplorer(*node.get()));
        }
    }

    device.setName(settings().name);
    auto addr = device.addressSettings();
    addr.name = settings().name;
    device.setAddressSettings(addr);

    auto dev = device.deviceSettings();
    dev.name = settings().name;
    device.setDeviceSettings(dev);

    return device;
}
