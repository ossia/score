#include "OSSIADevice.hpp"
#include <QDebug>


void OSSIADevice::addAddress(const FullAddressSettings &settings)
{
    using namespace OSSIA;
    qDebug() << Q_FUNC_INFO << "TODO";
    // Get the part of the address we want to add.
    // The device has already been removed.
    QStringList path = settings.name.split("/");
    path.removeFirst();

    // Find the relevant node to add in the device
    Node* node = m_dev.get();
    for(int i = 0; i < path.size(); i++)
    {
        auto it = std::find_if(node->begin(),
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

    std::shared_ptr<Address> addr;
    if(settings.valueType == "Float")
    { addr = node->createAddress(AddressValue::Type::FLOAT); }
    else if(settings.valueType == "Int")
    { addr = node->createAddress(AddressValue::Type::INT); }
    else if(settings.valueType == "String")
    { addr = node->createAddress(AddressValue::Type::STRING); }

    if(settings.ioType == "In")
    { addr->setAccessMode(Address::AccessMode::GET); }
    else if(settings.ioType == "Out")
    { addr->setAccessMode(Address::AccessMode::SET); }
    else if(settings.ioType == "In/Out")
    { addr->setAccessMode(Address::AccessMode::BI); }
}


void OSSIADevice::updateAddress(const AddressSettings &address)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}


void OSSIADevice::removeAddress(const QString &path)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}


void OSSIADevice::sendMessage(Message &mess)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}


bool OSSIADevice::check(const QString &str)
{
    return false;
}
