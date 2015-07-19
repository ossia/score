#include "iscore2OSSIA.hpp"

#include <boost/mpl/pair.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/at.hpp>
#include <boost/range/algorithm.hpp>

#include "Protocols/OSSIADevice.hpp"
namespace iscore
{
namespace convert
{
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


OSSIA::Node* findNodeFromPath(const QStringList& path, OSSIA::Device* dev)
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
        if(it != children.end())
            node = it->get();
        else
            return nullptr;
    }

    return node;

}

OSSIA::Node* getNodeFromPath(const QStringList &path, OSSIA::Device *dev)
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


void updateOSSIAAddress(const iscore::FullAddressSettings &settings, const std::shared_ptr<OSSIA::Address> &addr)
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


void createOSSIAAddress(const iscore::FullAddressSettings &settings, OSSIA::Node *node)
{
    using namespace OSSIA;
    std::shared_ptr<OSSIA::Address> addr;

    // Read the Qt docs on QVariant::type for the relationship with QMetaType::Type
    QMetaType::Type t = QMetaType::Type(settings.value.val.type());

    if(t == QMetaType::Float)
    { addr = node->createAddress(OSSIA::Value::Type::FLOAT); }

    else if(t == QMetaType::Int)
    { addr = node->createAddress(OSSIA::Value::Type::INT); }

    else if(t == QMetaType::QString)
    { addr = node->createAddress(OSSIA::Value::Type::STRING); }

    else if(t == QMetaType::Bool)
    { addr = node->createAddress(OSSIA::Value::Type::BOOL); }

    else if(t == QMetaType::Char)
    { addr = node->createAddress(OSSIA::Value::Type::CHAR); }

    else if(t == QMetaType::QVariantList)
    { addr = node->createAddress(OSSIA::Value::Type::TUPLE); }

    else if(t == QMetaType::QByteArray)
    { addr = node->createAddress(OSSIA::Value::Type::GENERIC); }

    updateOSSIAAddress(settings, addr);
}

void updateOSSIAValue(const QVariant& data, OSSIA::Value& val)
{
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
            ISCORE_TODO;
            /*
            const auto& array = data.value<QByteArray>();
            auto& generic = dynamic_cast<OSSIA::Generic&>(val);

            generic.size = array.size();

            delete generic.start;
            generic.start = new char[generic.size];

            boost::range::copy(array, generic.start);
            break;
            */
        }
        default:
            break;
    }
}


using OSSIATypeMap =
boost::mpl::map<
boost::mpl::pair<bool, OSSIA::Bool>,
boost::mpl::pair<int, OSSIA::Int>,
boost::mpl::pair<float, OSSIA::Float>,
boost::mpl::pair<char, OSSIA::Char>,
boost::mpl::pair<std::string, OSSIA::String>
>;
/*
using OSSIATypeEnumToTypeMap =
mpl::map<
mpl::pair<mpl::int_<(int)OSSIA::Value::Type::BOOL>, bool>,
mpl::pair<mpl::int_<(int)OSSIA::Value::Type::INT>, int>,
mpl::pair<mpl::int_<(int)OSSIA::Value::Type::FLOAT>, float>,
mpl::pair<mpl::int_<(int)OSSIA::Value::Type::CHAR>, char>,
mpl::pair<mpl::int_<(int)OSSIA::Value::Type::STRING>, std::string>
>;
*/

template<typename T>
OSSIA::Value* createOSSIAValue(const T& val)
{
    return new typename boost::mpl::at<OSSIATypeMap, T>::type(val);
}


OSSIA::Value* toValue(
        const iscore::Value& value)
{
    const QVariant& val = value.val;
    switch(QMetaType::Type(val.type()))
    {
        case QVariant::Type::Invalid:
            return new OSSIA::Impulse;
        case QMetaType::Type::Bool:
            return createOSSIAValue(val.value<bool>());
        case QMetaType::Type::Int:
            return createOSSIAValue(val.value<int>());
        case QMetaType::Type::Float:
            return createOSSIAValue(val.value<float>());
        case QMetaType::Type::Char:
            return createOSSIAValue(val.value<char>());
        case QMetaType::Type::QString:
            return createOSSIAValue(val.value<QString>().toStdString());
            //TODO tuple & generic

            /*
        case OSSIA::Value::Type::Tuple:
        {
            ISCORE_TODO;
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
            ISCORE_TODO;
            const auto& array = data.value<QByteArray>();
            auto& generic = dynamic_cast<OSSIA::Generic&>(val);

            generic.size = array.size();

            delete generic.start;
            generic.start = new char[generic.size];

            boost::range::copy(array, generic.start);
            break;
        }
            */
        default:
            break;
    }

    Q_ASSERT(false);
    return nullptr;
}

// TODO Handle the case where the message is not present.
std::shared_ptr<OSSIA::Message> message(const iscore::Message &mess, const DeviceList& deviceList)
{
    const auto& dev = deviceList.device(mess.address.device);

    if(auto casted_dev = dynamic_cast<const OSSIADevice*>(&dev))
    {
        auto ossia_node = iscore::convert::getNodeFromPath(
                    mess.address.path,
                    &casted_dev->impl());

        return OSSIA::Message::create(
                    ossia_node->getAddress(),
                    iscore::convert::toValue(mess.value));
    }

    return {};
}

std::shared_ptr<OSSIA::State> state(const iscore::State &iscore_state,  const DeviceList& deviceList)
{
    auto ossia_state = OSSIA::State::create();

    auto& elts = ossia_state->stateElements();

    if(iscore_state.data().canConvert<iscore::State>())
    {
        elts.push_back(state(iscore_state.data().value<iscore::State>(), deviceList));
    }
    else if(iscore_state.data().canConvert<iscore::StateList>())
    {
        for(const auto& st : iscore_state.data().value<iscore::StateList>())
        {
            elts.push_back(state(st, deviceList));
        }
    }
    else if(iscore_state.data().canConvert<iscore::Message>())
    {
        elts.push_back(message(iscore_state.data().value<iscore::Message>(), deviceList));
    }
    else if(iscore_state.data().canConvert<iscore::MessageList>())
    {
        for(const auto& mess : iscore_state.data().value<iscore::MessageList>())
        {
            elts.push_back(message(mess, deviceList));
        }
    }
    else
    {
        ISCORE_TODO;
    }

    return ossia_state;
}
}
}
