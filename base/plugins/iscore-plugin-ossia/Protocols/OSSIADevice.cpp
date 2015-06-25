#include "OSSIADevice.hpp"
#include <QDebug>
#include <boost/range/algorithm.hpp>
void OSSIADevice::addAddress(const FullAddressSettings &settings)
{
    using namespace OSSIA;
    // Get the node
    QStringList path = settings.name.split("/");
    path.removeFirst();
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
    path.removeFirst();

    OSSIA::Node* node = createNodeFromPath(path, m_dev.get());
    updateOSSIAAddress(settings, node->getAddress());
}


void OSSIADevice::removeAddress(const QString &address)
{
    using namespace OSSIA;
    QStringList path = address.split("/");
    path.removeFirst();
    path.removeFirst();

    OSSIA::Node* node = createNodeFromPath(path, m_dev.get());
    auto& children = node->getParent()->children();
    auto it = boost::range::find_if(children, [&] (auto&& elt) { return elt.get() == node; });
    if(it != children.end())
        children.erase(it);
}


void OSSIADevice::sendMessage(Message &mess)
{
    auto node = getNodeFromPath(mess.address.path, m_dev.get());

    auto val = node->getAddress()->getValue();
    updateOSSIAValue(mess.value, const_cast<OSSIA::Value&>(*val)); // TODO naye
    node->getAddress()->sendValue(val);
}


bool OSSIADevice::check(const QString &str)
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return false;
}


OSSIA::Node *createNodeFromPath(const QStringList &path, OSSIA::Device *dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::Node* node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = boost::range::find_if(
                      children,
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


OSSIA::Node *getNodeFromPath(const QStringList &path, OSSIA::Device *dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::Node* node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = boost::range::find_if(children,
                      [&] (const auto& ossia_node)
                    { return ossia_node->getName() == path[i].toStdString(); });
        Q_ASSERT(it != children.end());

        node = it->get();
    }

    Q_ASSERT(node);
    return node;
}


void updateOSSIAAddress(const FullAddressSettings &settings, const std::shared_ptr<OSSIA::Address> &addr)
{
    using namespace OSSIA;
    switch(settings.ioType)
    {
        case IOType::In:
            addr->setAccessMode(OSSIA::Address::AccessMode::GET);
            break;
        case IOType::Out:
            addr->setAccessMode(OSSIA::Address::AccessMode::SET);
            break;
        case IOType::InOut:
            addr->setAccessMode(OSSIA::Address::AccessMode::BI);
            break;
        case IOType::Invalid:
            // TODO There shouldn't be an address!!
            break;
    }
}


void createOSSIAAddress(const FullAddressSettings &settings, OSSIA::Node *node)
{
    using namespace OSSIA;
    std::shared_ptr<OSSIA::Address> addr;
    if(settings.value.type() == QMetaType::Float)
    { addr = node->createAddress(Value::Type::FLOAT); }

    else if(settings.value.type() == QMetaType::Int)
    { addr = node->createAddress(Value::Type::INT); }

    else if(settings.value.type() == QMetaType::QString)
    { addr = node->createAddress(Value::Type::STRING); }

    else if(settings.value.type() == QMetaType::Bool)
    { addr = node->createAddress(Value::Type::BOOL); }

    else if(settings.value.type() == QMetaType::Char)
    { addr = node->createAddress(Value::Type::CHAR); }

    else if(settings.value.type() == QMetaType::QVariantList)
    { addr = node->createAddress(Value::Type::TUPLE); }

    else if(settings.value.type() == QMetaType::QByteArray)
    { addr = node->createAddress(Value::Type::GENERIC); }

    updateOSSIAAddress(settings, addr);
}


IOType OSSIAAccessModeToIOType(OSSIA::Address::AccessMode t)
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


QVariant OSSIAValueToVariant(const OSSIA::Value *val)
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
                tuple.append(OSSIAValueToVariant(e));
            }

            v = tuple;
            break;
        }
        case OSSIA::Value::Type::GENERIC:
        {
            auto generic = dynamic_cast<const OSSIA::Generic*>(val);
            v = QByteArray{generic->start, generic->size};
            break;
        }
        default:
            break;
    }

    return v;
}
void updateOSSIAValue(const QVariant& data, OSSIA::Value& val)
{
    QVariant v;
    switch(val.getType())
    {
        case OSSIA::Value::Type::IMPULSE:
            break;
        case OSSIA::Value::Type::BOOL:
            dynamic_cast<OSSIA::Bool&>(val).value = data.value<bool>();
            break;
        case OSSIA::Value::Type::INT:
            dynamic_cast<OSSIA::Int&>(val).value = data.value<int>();
            break;
        case OSSIA::Value::Type::FLOAT:
            dynamic_cast<OSSIA::Float&>(val).value = data.value<float>();
            break;
        case OSSIA::Value::Type::CHAR:
            dynamic_cast<OSSIA::Char&>(val).value = data.value<char>();
            break;
        case OSSIA::Value::Type::STRING:
            dynamic_cast<OSSIA::String&>(val).value = data.value<QString>().toStdString();
            break;
        case OSSIA::Value::Type::TUPLE:
        {
            QVariantList tuple = data.value<QVariantList>();
            const auto& vec = dynamic_cast<OSSIA::Tuple&>(val).value;
            Q_ASSERT(tuple.size() == (int)vec.size());
            for(int i = 0; i < (int)vec.size(); i++)
            {
                updateOSSIAValue(tuple[i], *vec[i]);
            }

            break;
        }
        case OSSIA::Value::Type::GENERIC:
        {
            const auto& array = data.value<QByteArray>();
            auto& generic = dynamic_cast<OSSIA::Generic&>(val);

            generic.size = array.size();

            delete generic.start;
            generic.start = new char[generic.size];

            boost::range::copy(array, generic.start);
            break;
        }
        default:
            break;
    }
}




AddressSettings extractAddressSettings(const OSSIA::Node &node)
{
    AddressSettings s;
    const auto& addr = node.getAddress();
    s.name = QString::fromStdString(node.getName());

    if(addr)
    {
        s.value = OSSIAValueToVariant(addr->getValue());
        s.ioType = OSSIAAccessModeToIOType(addr->getAccessMode());
        s.clipMode = OSSIABoudingModeToClipMode(addr->getBoundingMode());
        s.repetitionFilter = addr->getRepetitionFilter();
        s.domain = OSSIADomainToDomain(*addr->getDomain());
    }
    return s;
}


Node *OssiaToDeviceExplorer(const OSSIA::Node &node)
{
    Node* n = new Node{extractAddressSettings(node)};

    // 2. Recurse on the children
    for(const auto& ossia_child : node.children())
    {
        n->addChild(OssiaToDeviceExplorer(*ossia_child.get()));
    }

    return n;
}


Domain OSSIADomainToDomain(OSSIA::Domain &domain)
{
    Domain d;
    d.min = OSSIAValueToVariant(domain.getMin());
    d.max = OSSIAValueToVariant(domain.getMax());

    for(const auto& val : domain.getValues())
    {
        d.values.append(OSSIAValueToVariant(val));
    }

    return d;
}
