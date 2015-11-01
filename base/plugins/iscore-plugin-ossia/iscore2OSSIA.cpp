#include "iscore2OSSIA.hpp"

#include <Process/State/MessageNode.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/at.hpp>
#include <boost/range/algorithm.hpp>

#include <Misc/CallbackContainer.h>
#include <Editor/Expression.h>
#include <Editor/ExpressionAtom.h>
#include <Editor/ExpressionComposition.h>
#include <Editor/ExpressionNot.h>

#include "Protocols/OSSIADevice.hpp"
#include <QMap>


class NodeNotFoundException : public std::exception
{
        const iscore::Address& m_addr;
    public:
        NodeNotFoundException(const iscore::Address& n):
            m_addr{n}
        {

        }

        const char* what() const noexcept override
        {
            return QString("Address: %1 not found in actual tree.")
                    .arg(m_addr.toString()).toUtf8().constData();
        }
};

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
                if(path[k].toStdString() != (*newNodeIt)->getName())
                {
                    qDebug() << path[k] << (*newNodeIt)->getName().c_str();
                    for(const auto& node : parentnode->children())
                    {
                        qDebug() << node->getName().c_str();
                    }
                    ISCORE_ABORT;
                }
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
        auto it = std::find_if(children.begin(), children.end(),
                               [&] (const auto& ossia_node)
        { return ossia_node->getName() == path[i].toStdString(); });
        if(it != children.end())
            node = it->get();
        else
            return nullptr;
    }

    return node;
}

std::shared_ptr<OSSIA::Node> findNodeFromPath(const QStringList& path, std::shared_ptr<OSSIA::Device> dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    std::shared_ptr<OSSIA::Node> node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = std::find_if(children.begin(), children.end(),
                               [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });
        if(it != children.end())
            node = *it;
        else
            return {};
    }

    return node;
}

OSSIA::Node* getNodeFromPath(
        const QStringList &path,
        OSSIA::Device *dev)
{
    using namespace OSSIA;
    // Find the relevant node to add in the device
    OSSIA::Node* node = dev;
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();

        auto it = boost::range::find_if(children,
                                        [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });

        ISCORE_ASSERT(it != children.end());

        node = it->get();
    }

    ISCORE_ASSERT(node);
    return node;
}


void updateOSSIAAddress(
        const iscore::FullAddressSettings &settings,
        const std::shared_ptr<OSSIA::Address> &addr)
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

    auto ossia_val = addr->getValue();
    if(ossia_val)
    {
        auto val = addr->getValue()->clone();

        updateOSSIAValue(settings.value.val,*val);
        addr->pushValue(val);
        delete val;
    }
}

void createOSSIAAddress(
        const iscore::FullAddressSettings &settings,
        OSSIA::Node *node)
{
    if(settings.ioType == IOType::Invalid)
        return;

    using namespace OSSIA;

    static const constexpr struct {
        public:
            using return_type = OSSIA::Value::Type;
            return_type operator()(const no_value_t&) const { ISCORE_ABORT; return OSSIA::Value::Type::IMPULSE; }
            return_type operator()(const impulse_t&) const { return OSSIA::Value::Type::IMPULSE; }
            return_type operator()(int) const { return OSSIA::Value::Type::INT; }
            return_type operator()(float) const { return OSSIA::Value::Type::FLOAT; }
            return_type operator()(bool) const { return OSSIA::Value::Type::BOOL; }
            return_type operator()(const QString&) const { return OSSIA::Value::Type::STRING; }
            return_type operator()(const QChar&) const { return OSSIA::Value::Type::CHAR; }
            return_type operator()(const tuple_t&) const { return OSSIA::Value::Type::TUPLE; }
    } visitor{};

    updateOSSIAAddress(settings,  node->createAddress(eggs::variants::apply(visitor, settings.value.val.impl())));
}

void updateOSSIAValue(const iscore::ValueImpl& data, OSSIA::Value& val)
{
    switch(val.getType())
    {
        case OSSIA::Value::Type::IMPULSE:
            break;
        case OSSIA::Value::Type::BOOL:
            safe_cast<OSSIA::Bool&>(val).value = data.get<bool>();
            break;
        case OSSIA::Value::Type::INT:
            safe_cast<OSSIA::Int&>(val).value = data.get<int>();
            break;
        case OSSIA::Value::Type::FLOAT:
            safe_cast<OSSIA::Float&>(val).value = data.get<float>();
            break;
        case OSSIA::Value::Type::CHAR: // TODO See TODO in MessageEditDialog
            safe_cast<OSSIA::Char&>(val).value = data.get<QChar>().toLatin1();
            break;
        case OSSIA::Value::Type::STRING:
            safe_cast<OSSIA::String&>(val).value = data.get<QString>().toStdString();
            break;
        case OSSIA::Value::Type::TUPLE:
        {
            tuple_t tuple = data.get<tuple_t>();
            const auto& vec = safe_cast<OSSIA::Tuple&>(val).value;
            ISCORE_ASSERT(tuple.size() == vec.size());
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
        case OSSIA::Value::Type::DESTINATION:
        case OSSIA::Value::Type::BEHAVIOR:
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

static OSSIA::Value* toValue(const iscore::ValueImpl& val)
{
    static const constexpr struct {
        public:
            using return_type = OSSIA::Value*;
            return_type operator()(const no_value_t&) const { ISCORE_ABORT; return nullptr; }
            return_type operator()(const impulse_t&) const { return new OSSIA::Impulse; }
            return_type operator()(int v) const { return new OSSIA::Int{v}; }
            return_type operator()(float v) const { return new OSSIA::Float{v}; }
            return_type operator()(bool v) const { return new OSSIA::Bool{v}; }
            return_type operator()(const QString& v) const { return new OSSIA::String{v.toStdString()}; }
            return_type operator()(const QChar& v) const { return new OSSIA::Char{v.toLatin1()}; }
            return_type operator()(const tuple_t& v) const
            {
                auto ossia_tuple = new OSSIA::Tuple;
                for(const auto& val : v)
                {
                    ossia_tuple->value.push_back(eggs::variants::apply(*this, val.impl()));
                }
                return ossia_tuple;
            }
    } visitor{};

    return eggs::variants::apply(visitor, val.impl());
}

OSSIA::Value* toValue(
        const iscore::Value& value)
{
    return toValue(value.val);
}

std::shared_ptr<OSSIA::Message> message(
        const iscore::Message& mess,
        const DeviceList& deviceList)
{
    if(!deviceList.hasDevice(mess.address.device))
        return {};

    // OPTIMIZEME by sorting by device prior
    // to this.
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

template<typename Fun>
static void visit_node(
        const MessageNode& root,
        Fun f)
{
    f(root);

    for(const auto& child : root.children())
    {
        visit_node(child, f);
    }
}

std::shared_ptr<OSSIA::State> state(
        std::shared_ptr<OSSIA::State> ossia_state,
        const MessageNode &iscore_state,
        const DeviceList& deviceList)
{
    auto& elts = ossia_state->stateElements();

    // For all elements where IOType != Invalid,
    // we add the elements to the state.

    visit_node(iscore_state, [&] (const MessageNode& n) {
            const auto& val = n.value();
            if(val)
            {
                elts.push_back(
                            message(
                                iscore::Message{
                                    address(n),
                                    *val},
                                deviceList
                                )
                            );
            }
    });

    return ossia_state;
}




OSSIA::Value* expressionOperand(
        const iscore::RelationMember& relm,
        const DeviceList& devlist)
{
    using namespace eggs::variants;
    switch(relm.which())
    {
        case 0:
        {
            const auto& addr = get<iscore::Address>(relm);
            if(!devlist.hasDevice(addr.device))
            {
                throw NodeNotFoundException(addr);
            }

            auto& device = devlist.device(addr.device);

            if(auto casted_dev = dynamic_cast<const OSSIADevice*>(&device))
            {
                auto n = findNodeFromPath(addr.path, casted_dev->impl_ptr());
                if(n)
                {
                    return new OSSIA::Destination(n);
                }
                else
                {
                    throw NodeNotFoundException(addr);
                }
            }
            else
            {
                throw NodeNotFoundException(addr);
            }

            break;
        }
        case 1:
        {
            return toValue( get<iscore::Value>(relm));
            break;
        }
        default:
            ISCORE_ABORT;
    }
}

OSSIA::ExpressionAtom::Operator expressionOperator(iscore::Relation::Operator op)
{
    switch(op)
    {
        case iscore::Relation::Operator::Different:
            return OSSIA::ExpressionAtom::Operator::DIFFERENT;
        case iscore::Relation::Operator::Equal:
            return OSSIA::ExpressionAtom::Operator::EQUAL;
        case iscore::Relation::Operator::Greater:
            return OSSIA::ExpressionAtom::Operator::GREATER_THAN;
        case iscore::Relation::Operator::GreaterEqual:
            return OSSIA::ExpressionAtom::Operator::GREATER_THAN_OR_EQUAL;
        case iscore::Relation::Operator::Lower:
            return OSSIA::ExpressionAtom::Operator::LOWER_THAN;
        case iscore::Relation::Operator::LowerEqual:
            return OSSIA::ExpressionAtom::Operator::LOWER_THAN_OR_EQUAL;
        default:
            ISCORE_ABORT;
    }
}

// iscore::Relation -> OSSIA::ExpressionAtom
std::shared_ptr<OSSIA::ExpressionAtom> expressionAtom(
        const iscore::Relation& rel,
        const DeviceList& dev)
{
    using namespace eggs::variants;

    return OSSIA::ExpressionAtom::create(
                expressionOperand(rel.lhs, dev),
                expressionOperator(rel.op),
                expressionOperand(rel.rhs, dev));
}

static const QMap<iscore::BinaryOperator, OSSIA::ExpressionComposition::Operator> comp_map
{
    {iscore::BinaryOperator::And, OSSIA::ExpressionComposition::Operator::AND},
    {iscore::BinaryOperator::Or, OSSIA::ExpressionComposition::Operator::OR},
    {iscore::BinaryOperator::Xor, OSSIA::ExpressionComposition::Operator::XOR}
};

std::shared_ptr<OSSIA::Expression> expression(
        const iscore::Expression& expr,
        const DeviceList& devlist)
{
    if(expr.is<InvisibleRootNodeTag>())
    {
        if(expr.childCount() == 0)
        {
            // By default no expression == true
            return OSSIA::Expression::create(true);
        }
        else if(expr.childCount() == 1)
        {
            return expression(expr.childAt(0), devlist);
        }
        else
        {
            ISCORE_ABORT;
        }
    }
    else if(expr.is<iscore::Relation>())
    {
        return expressionAtom(expr.get<iscore::Relation>(), devlist);
    }
    else if(expr.is<iscore::BinaryOperator>())
    {
        const auto& lhs = expr.childAt(0);
        const auto& rhs = expr.childAt(1);
        return OSSIA::ExpressionComposition::create(
                    expression(lhs, devlist),
                    comp_map[expr.get<iscore::BinaryOperator>()],
                    expression(rhs, devlist)
                    );

    }
    else if(expr.is<iscore::UnaryOperator>())
    {
        return OSSIA::ExpressionNot::create(expression(expr.childAt(0), devlist));
    }
    else
    {
        ISCORE_ABORT;
        return {};
    }
}

void removeOSSIAAddress(OSSIA::Node* n)
{
    n->removeAddress();
}

}
}
