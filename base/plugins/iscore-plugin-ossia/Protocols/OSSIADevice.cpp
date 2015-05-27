#include "OSSIADevice.hpp"
#include <QDebug>

// Gets a node from an address in a device.
// Creates it if necessary.
OSSIA::Node* nodeFromPath(QStringList path, OSSIA::Device* dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::Node* node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = std::find_if(
                    children.begin(),
                    children.end(),
                    [&] (const auto& ossia_node) { return ossia_node->getName() == path[i].toStdString(); });
        if(it == children.end())
        {
            // We have to start adding sub-nodes from here.
            OSSIA::Node* parentnode = node;
            for(int k = i; k < path.size(); k++)
            {
                auto newNodeIt = parentnode->emplace(parentnode->children().begin(), path[k].toStdString());
                if(k == path.size() - 1)
                {
                    node = newNodeIt->get();
                }
                else
                {
                    parentnode = newNodeIt->get();
                }
            }

            break;
        }
        else
        {
            node = it->get();
        }
    }

    return node;
}

void updateAddressSettings(const FullAddressSettings& settings, const std::shared_ptr<OSSIA::Address>& addr)
{
    using namespace OSSIA;
    switch(settings.ioType)
    {
        case IOType::In:
            addr->setAccessMode(Address::AccessMode::GET);
            break;
        case IOType::Out:
            addr->setAccessMode(Address::AccessMode::SET);
            break;
        case IOType::InOut:
            addr->setAccessMode(Address::AccessMode::BI);
            break;
        case IOType::Invalid:
            // TODO There shouldn't be an address!!
            break;
    }
}

void createAddressSettings(const FullAddressSettings& settings, OSSIA::Node* node)
{
    using namespace OSSIA;
    std::shared_ptr<Address> addr;
    if(settings.valueType == "Float")
    { addr = node->createAddress(AddressValue::Type::FLOAT); }
    else if(settings.valueType == "Int")
    { addr = node->createAddress(AddressValue::Type::INT); }
    else if(settings.valueType == "String")
    { addr = node->createAddress(AddressValue::Type::STRING); }

    updateAddressSettings(settings, addr);
}

void OSSIADevice::addAddress(const FullAddressSettings &settings)
{
    using namespace OSSIA;
    // Get the node
    QStringList path = settings.name.split("/");
    path.removeFirst();
    path.removeFirst();

    // Create it
    OSSIA::Node* node = nodeFromPath(path, m_dev.get());

    // Populate the node with an address
    createAddressSettings(settings, node);
}


void OSSIADevice::updateAddress(const FullAddressSettings &settings)
{
    using namespace OSSIA;
    QStringList path = settings.name.split("/");
    path.removeFirst();
    path.removeFirst();

    OSSIA::Node* node = nodeFromPath(path, m_dev.get());
    updateAddressSettings(settings, node->getAddress());
}


void OSSIADevice::removeAddress(const QString &address)
{
    using namespace OSSIA;
    QStringList path = address.split("/");
    path.removeFirst();
    path.removeFirst();

    OSSIA::Node* node = nodeFromPath(path, m_dev.get());
    auto& children = node->getParent().children();
    auto it = std::find_if(children.begin(), children.end(), [&] (auto&& elt) { return elt.get() == node; });
    if(it != children.end())
        children.erase(it);
}


void OSSIADevice::sendMessage(Message &mess)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}


bool OSSIADevice::check(const QString &str)
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return false;
}
