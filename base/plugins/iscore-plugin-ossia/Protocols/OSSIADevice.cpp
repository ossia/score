#include "OSSIADevice.hpp"
#include <QDebug>
#include <boost/range/algorithm.hpp>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;

void OSSIADevice::addAddress(const iscore::FullAddressSettings &settings)
{
    using namespace OSSIA;

    // Create the node. It is added into the device.
    OSSIA::Node* node = createNodeFromPath(settings.address.path, m_dev.get());

    // Populate the node with an address. Won't add one if IOType == Invalid.
    createOSSIAAddress(settings, node);
}


void OSSIADevice::updateAddress(const iscore::FullAddressSettings &settings)
{
    using namespace OSSIA;

    OSSIA::Node* node = getNodeFromPath(settings.address.path, m_dev.get());

    if(settings.ioType == iscore::IOType::Invalid)
        removeOSSIAAddress(node);
    else
        updateOSSIAAddress(settings, node->getAddress());
}


void OSSIADevice::removeAddress(const iscore::Address& address)
{
    using namespace OSSIA;

    OSSIA::Node* node = getNodeFromPath(address.path, m_dev.get());
    auto& children = node->getParent()->children();
    auto it = std::find_if(children.begin(), children.end(),
                           [&] (auto&& elt) { return elt.get() == node; });
    if(it != children.end())
    {
        children.erase(it);
    }
}


void OSSIADevice::sendMessage(iscore::Message &mess)
{
    auto node = getNodeFromPath(mess.address.path, m_dev.get());

    auto val = node->getAddress()->getValue()->clone();

    updateOSSIAValue(mess.value.val,*val);
    node->getAddress()->pushValue(val);
}


bool OSSIADevice::check(const QString &str)
{
    ISCORE_TODO;
    return false;
}

OSSIA::Device& OSSIADevice::impl() const
{
    return *m_dev;
}

