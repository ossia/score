#include "OSSIADevice.hpp"
#include <QDebug>


OSSIA::Node* nodeFromPath(QStringList path, OSSIA::Device* dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    Node* node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        auto it = std::find_if(
                    node->begin(),
                    node->end(),
                    [&] (const Node& n) { return n.getName() == path[i].toStdString(); });
        if(it == node->end())
        {
            // We have to start adding sub-nodes from here.
            Node* parentnode = node;
            for(int k = i; k < path.size(); k++)
            {
                auto theNode = parentnode->emplace(parentnode->begin(), path[k].toStdString());
                if(k == path.size() - 1)
                {
                    node = theNode;
                }
                else
                {
                    parentnode = theNode;
                }
            }

            break;
        }
        else
        {
            node = it;
        }
    }

    return node;
}

void updateAddressSettings(const FullAddressSettings& settings, const std::shared_ptr<OSSIA::Address>& addr)
{
    using namespace OSSIA;
    if(settings.ioType == "In")
    { addr->setAccessMode(Address::AccessMode::GET); }
    else if(settings.ioType == "Out")
    { addr->setAccessMode(Address::AccessMode::SET); }
    else if(settings.ioType == "In/Out")
    { addr->setAccessMode(Address::AccessMode::BI); }
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

    Node* node = nodeFromPath(path, m_dev.get());
    updateAddressSettings(settings, node->getAddress());
}


void OSSIADevice::removeAddress(const QString &address)
{
    using namespace OSSIA;
    QStringList path = address.split("/");
    path.removeFirst();

    Node* node = nodeFromPath(path, m_dev.get());
    node->getParent().erase(node);
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
