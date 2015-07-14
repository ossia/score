#include "OSSIADevice.hpp"
#include <QDebug>
#include <boost/range/algorithm.hpp>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"
using namespace iscore::convert;
using namespace OSSIA::convert;

void OSSIADevice::addAddress(const FullAddressSettings &settings)
{
    using namespace OSSIA;
    // Get the node
    QStringList path = settings.name.split("/");
    path.removeFirst();

    // Create it
    OSSIA::Node* node = createNodeFromPath(path, m_dev.get());

    // Populate the node with an address
    createOSSIAAddress(settings, node);
}


void OSSIADevice::updateAddress(const FullAddressSettings &settings)
{
    using namespace OSSIA;
    QStringList path = settings.name.split("/");
    path.removeFirst();

    OSSIA::Node* node = createNodeFromPath(path, m_dev.get());
    updateOSSIAAddress(settings, node->getAddress());
}


void OSSIADevice::removeAddress(const QString &address)
{
    using namespace OSSIA;
    QStringList path = address.split("/");
    path.removeFirst();

    OSSIA::Node* node = createNodeFromPath(path, m_dev.get());
    auto& children = node->getParent()->children();
    auto it = boost::range::find_if(children, [&] (auto&& elt) { return elt.get() == node; });
    if(it != children.end())
        children.erase(it);
}


void OSSIADevice::sendMessage(iscore::Message &mess)
{
    auto node = getNodeFromPath(mess.address.path, m_dev.get());

    auto val = node->getAddress()->getValue();
    updateOSSIAValue(mess.value, const_cast<OSSIA::Value&>(*val)); // TODO naye
    node->getAddress()->sendValue(val);
}


bool OSSIADevice::check(const QString &str)
{
    ISCORE_TODO
    return false;
}

OSSIA::Device& OSSIADevice::impl() const
{
    return *m_dev;
}

