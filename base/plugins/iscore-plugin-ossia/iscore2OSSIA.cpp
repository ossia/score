#include "iscore2OSSIA.hpp"

#include <boost/mpl/pair.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/at.hpp>
#include <boost/range/algorithm.hpp>

#include "Protocols/OSSIADevice.hpp"
#include <QMap>
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
        ISCORE_ASSERT(it != children.end());

        node = it->get();
    }

    ISCORE_ASSERT(node);
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
            ISCORE_ABORT;
            break;
    }
}

void createOSSIAAddress(const iscore::FullAddressSettings &settings, OSSIA::Node *node)
{
    if(settings.ioType == IOType::Invalid)
        return;

    using namespace OSSIA;
    std::shared_ptr<OSSIA::Address> addr;

    // Read the Qt docs on QVariant::type for the relationship with QMetaType::Type
    QMetaType::Type t = static_cast<QMetaType::Type>(settings.value.val.type());

    static const QMap<QMetaType::Type, OSSIA::Value::Type> type_map{
        {QMetaType::UnknownType, OSSIA::Value::Type::IMPULSE},
        {QMetaType::Float, OSSIA::Value::Type::FLOAT},
        {QMetaType::Int, OSSIA::Value::Type::INT},
        {QMetaType::QString, OSSIA::Value::Type::STRING},
        {QMetaType::Bool, OSSIA::Value::Type::BOOL},
        {QMetaType::QChar, OSSIA::Value::Type::CHAR},
        {QMetaType::QVariantList, OSSIA::Value::Type::TUPLE},
        {QMetaType::QByteArray, OSSIA::Value::Type::GENERIC}
    };

    ISCORE_ASSERT(type_map.keys().contains(t));

    updateOSSIAAddress(settings,  node->createAddress(type_map[t]));

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
            dynamic_cast<OSSIA::Char&>(val).value = data.value<QChar>().toLatin1();
            break;
        case OSSIA::Value::Type::STRING:
            dynamic_cast<OSSIA::String&>(val).value = data.value<QString>().toStdString();
            break;
        case OSSIA::Value::Type::TUPLE:
        {
            QVariantList tuple = data.value<QVariantList>();
            const auto& vec = dynamic_cast<OSSIA::Tuple&>(val).value;
            ISCORE_ASSERT(tuple.size() == (int)vec.size());
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

template<typename T>
OSSIA::Value* createOSSIAValue(const T& val)
{
    return new typename boost::mpl::at<OSSIATypeMap, T>::type(val);
}

static OSSIA::Value* toValue(const QVariant& val)
{
    switch(static_cast<QMetaType::Type>(val.type()))
    {
        case QMetaType::Type::UnknownType: // == QVariant::Invalid
            return new OSSIA::Impulse;
        case QMetaType::Type::Bool:
            return createOSSIAValue(val.value<bool>());
        case QMetaType::Type::Int:
            return createOSSIAValue(val.value<int>());
        case QMetaType::Type::Float:
            return createOSSIAValue(val.value<float>());
        case QMetaType::Type::Double:
            return createOSSIAValue((float)val.value<double>());
        case QMetaType::Type::QChar:
            return createOSSIAValue(val.value<QChar>().toLatin1());
        case QMetaType::Type::QString:
            return createOSSIAValue(val.value<QString>().toStdString());
        case QMetaType::Type::QVariantList:
        {
            QVariantList tuple = val.value<QVariantList>();
            auto ossia_tuple = new OSSIA::Tuple;
            for(const auto& val : tuple)
            {
                ossia_tuple->value.push_back(toValue(val));
            }
            return ossia_tuple;
            break;
        }
        case QMetaType::Type::QByteArray:
        {
            //return createOSSIAValue(val.value<QByteArray>());
            ISCORE_TODO;
            /*
            const auto& array = data.value<QByteArray>();
            auto& generic = dynamic_cast<OSSIA::Generic&>(val);

            generic.size = array.size();

            delete generic.start;
            generic.start = new char[generic.size];

            boost::range::copy(array, generic.start);
            */
            break;
        }
        default:
            break;
    }


    ISCORE_BREAKPOINT;
    ISCORE_ABORT;
    return nullptr;
}

OSSIA::Value* toValue(
        const iscore::Value& value)
{
    return toValue(value.val);
}

std::shared_ptr<OSSIA::Message> message(const iscore::Message& mess, const DeviceList& deviceList)
{
    if(!deviceList.hasDevice(mess.address.device))
        return {};

    const auto& dev = deviceList.device(mess.address.device);

    if(auto casted_dev = dynamic_cast<const OSSIADevice*>(&dev))
    {
        auto ossia_node = iscore::convert::findNodeFromPath(
                    mess.address.path,
                    &casted_dev->impl());

        if(!ossia_node)
            return {};

        return OSSIA::Message::create(
                    ossia_node->getAddress(),
                    iscore::convert::toValue(mess.value));
    }

    return {};
}

std::shared_ptr<OSSIA::State> state(
        const iscore::StateNode &iscore_state,
        const DeviceList& deviceList)
{
    auto ossia_state = OSSIA::State::create();

    auto& elts = ossia_state->stateElements();

    if(iscore_state.is<InvisibleRootNodeTag>())
    {
        // Do nothing
    }
    else if(iscore_state.is<MessageList>())
    {
        for(const auto& mess : iscore_state.get<MessageList>())
        {
            elts.push_back(message(mess, deviceList));
        }
    }
    else
    {
        ISCORE_TODO;
    }

    for(const auto& child : iscore_state.children())
    {
        elts.push_back(state(*child, deviceList));
    }

    return ossia_state;
}
}
}
