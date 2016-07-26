#include <Editor/Expression.h>
#include <Editor/ExpressionAtom.h>
#include <Editor/ExpressionComposition.h>
#include <Editor/ExpressionNot.h>
#include <Process/State/MessageNode.hpp>
#include <boost/concept/usage.hpp>

#include <boost/mpl/at.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/pair.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <eggs/variant/variant.hpp>
#include <QByteArray>
#include <QChar>
#include <QDebug>
#include <QList>
#include <QMap>
#include <QString>
#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include "Editor/Message.h"
#include "Editor/State.h"
#include <Editor/Value/Value.h>
#include <Editor/ExpressionPulse.h>
#include "Network/Address.h"
#include "Network/Device.h"
#include "Network/Node.h"
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <State/Address.hpp>
#include <State/Expression.hpp>
#include <State/Message.hpp>
#include <State/Relation.hpp>
#include <iscore/tools/InvisibleRootNode.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/StateProcessComponent.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>

#include <boost/call_traits.hpp>
class NodeNotFoundException : public std::exception
{
        State::Address m_addr;
    public:
        NodeNotFoundException(const State::Address& n):
            m_addr(n)
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
        auto it = find_if(children,
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

        auto it = find_if(children, [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });

        ISCORE_ASSERT(it != children.end());

        node = it->get();
    }

    ISCORE_ASSERT(node);
    return node;
}

static OSSIA::BoundingMode toBoundingMode(Device::ClipMode c)
{
    switch(c)
    {
        case Device::ClipMode::Free:
            return OSSIA::BoundingMode::FREE;
        case Device::ClipMode::Clip:
            return OSSIA::BoundingMode::CLIP;
        case Device::ClipMode::Wrap:
            return OSSIA::BoundingMode::WRAP;
        case Device::ClipMode::Fold:
            return OSSIA::BoundingMode::FOLD;
        default:
            ISCORE_ABORT;
            return static_cast<OSSIA::BoundingMode>(-1);
    }
}

void updateOSSIAAddress(
        const Device::FullAddressSettings &settings,
        const std::shared_ptr<OSSIA::Address> &addr)
{
    switch(settings.ioType)
    {
        case Device::IOType::In:
            addr->setAccessMode(OSSIA::AccessMode::GET);
            break;
        case Device::IOType::Out:
            addr->setAccessMode(OSSIA::AccessMode::SET);
            break;
        case Device::IOType::InOut:
            addr->setAccessMode(OSSIA::AccessMode::BI);
            break;
        case Device::IOType::Invalid:
            ISCORE_ABORT;
            break;
    }

    addr->setRepetitionFilter(settings.repetitionFilter);
    addr->setBoundingMode(toBoundingMode(settings.clipMode));

    auto val = iscore::convert::toOSSIAValue(settings.value);
    addr->setValue(*val);

    auto min = toOSSIAValue(settings.domain.min);
    auto max = toOSSIAValue(settings.domain.max);
    if(min && max)
    {
        addr->setDomain(OSSIA::Domain::create(min.get(), max.get()));
    }
}

void createOSSIAAddress(
        const Device::FullAddressSettings &settings,
        OSSIA::Node *node)
{
    if(settings.value.val.is<State::no_value_t>())
        return;

    struct {
        public:
            using return_type = OSSIA::Type;
            return_type operator()(const State::no_value_t&) const { ISCORE_ABORT; return OSSIA::Type::IMPULSE; }
            return_type operator()(const State::impulse_t&) const { return OSSIA::Type::IMPULSE; }
            return_type operator()(int) const { return OSSIA::Type::INT; }
            return_type operator()(float) const { return OSSIA::Type::FLOAT; }
            return_type operator()(bool) const { return OSSIA::Type::BOOL; }
            return_type operator()(const QString&) const { return OSSIA::Type::STRING; }
            return_type operator()(const QChar&) const { return OSSIA::Type::CHAR; }
            return_type operator()(const State::vec2f&) const { return OSSIA::Type::VEC2F; }
            return_type operator()(const State::vec3f&) const { return OSSIA::Type::VEC3F; }
            return_type operator()(const State::vec4f&) const { return OSSIA::Type::VEC4F; }
            return_type operator()(const State::tuple_t&) const { return OSSIA::Type::TUPLE; }
    } visitor{};

    updateOSSIAAddress(settings,  node->createAddress(eggs::variants::apply(visitor, settings.value.val.impl())));
}

void updateOSSIAValue(const State::ValueImpl& data, OSSIA::Value& val)
{
    switch(val.getType())
    {
        case OSSIA::Type::IMPULSE:
            break;
        case OSSIA::Type::BOOL:
            safe_cast<OSSIA::Bool&>(val).value = data.get<bool>();
            break;
        case OSSIA::Type::INT:
            safe_cast<OSSIA::Int&>(val).value = data.get<int>();
            break;
        case OSSIA::Type::FLOAT:
            safe_cast<OSSIA::Float&>(val).value = data.get<float>();
            break;
        case OSSIA::Type::CHAR: // TODO See TODO in MessageEditDialog
            safe_cast<OSSIA::Char&>(val).value = data.get<QChar>().toLatin1();
            break;
        case OSSIA::Type::STRING:
            safe_cast<OSSIA::String&>(val).value = data.get<QString>().toStdString();
            break;
        case OSSIA::Type::VEC2F:
        {
            safe_cast<OSSIA::Vec2f&>(val).value = data.get<State::vec2f>();
            break;
        }
        case OSSIA::Type::VEC3F:
        {
            safe_cast<OSSIA::Vec3f&>(val).value = data.get<State::vec3f>();
            break;
        }
        case OSSIA::Type::VEC4F:
        {
            safe_cast<OSSIA::Vec4f&>(val).value = data.get<State::vec4f>();
            break;
        }
        case OSSIA::Type::TUPLE:
        {
            State::tuple_t tuple = data.get<State::tuple_t>();
            const auto& vec = safe_cast<OSSIA::Tuple&>(val).value;
            ISCORE_ASSERT(tuple.size() == vec.size());
            int n = vec.size();
            for(int i = 0; i < n; i++)
            {
                updateOSSIAValue(tuple[i], *vec[i]);
            }

            break;
        }
        case OSSIA::Type::DESTINATION:
        case OSSIA::Type::BEHAVIOR:
        default:
            break;
    }
}

static std::unique_ptr<OSSIA::Value> toOSSIAValue(const State::ValueImpl& val)
{
    struct {
        public:
            using return_type = OSSIA::Value*;
            return_type operator()(const State::no_value_t&) const { return nullptr; }
            return_type operator()(const State::impulse_t&) const { return new OSSIA::Impulse; }
            return_type operator()(int v) const { return new OSSIA::Int{v}; }
            return_type operator()(float v) const { return new OSSIA::Float{v}; }
            return_type operator()(bool v) const { return new OSSIA::Bool{v}; }
            return_type operator()(const QString& v) const { return new OSSIA::String{v.toStdString()}; }
            return_type operator()(const QChar& v) const { return new OSSIA::Char{v.toLatin1()}; }
            return_type operator()(const State::vec2f& v) const { return new OSSIA::Vec2f{v}; }
            return_type operator()(const State::vec3f& v) const { return new OSSIA::Vec3f{v}; }
            return_type operator()(const State::vec4f& v) const { return new OSSIA::Vec4f{v}; }
            return_type operator()(const State::tuple_t& v) const
            {
                auto ossia_tuple = new OSSIA::Tuple;
                for(const auto& tuple_elt : v)
                {
                    ossia_tuple->value.push_back(eggs::variants::apply(*this, tuple_elt.impl()));
                }
                return ossia_tuple;
            }
    } visitor{};

    return std::unique_ptr<OSSIA::Value>{eggs::variants::apply(visitor, val.impl())};
}

std::unique_ptr<OSSIA::Value> toOSSIAValue(
        const State::Value& value)
{
    return toOSSIAValue(value.val);
}

std::shared_ptr<OSSIA::Message> message(
        const State::Message& mess,
        const Device::DeviceList& deviceList)
{
    auto it = deviceList.find(mess.address.device);
    if(it == deviceList.devices().end())
        return {};

    // OPTIMIZEME by sorting by device prior
    // to this.
    const auto& dev = **it;
    if(!dev.connected())
        return {};

    if(auto casted_dev = dynamic_cast<const Ossia::OSSIADevice*>(&dev))
    {
        auto ossia_node = iscore::convert::findNodeFromPath(
                    mess.address.path,
                    &casted_dev->impl());

        if(!ossia_node)
            return {};
        auto ossia_addr = ossia_node->getAddress();
        if (!ossia_addr)
            return{};

        auto val = iscore::convert::toOSSIAValue(mess.value);
        if(!val)
            return {};

        return OSSIA::Message::create(ossia_addr, *val);
    }

    return {};
}

template<typename Fun>
static void visit_node(
        const Process::MessageNode& root,
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
        const Scenario::StateModel& iscore_state,
        const RecreateOnPlay::Context& ctx
        )
{
    auto& elts = ossia_state->stateElements();

    // For all elements where IOType != Invalid,
    // we add the elements to the state.

    auto& dl = ctx.devices.list();
    visit_node(iscore_state.messages().rootNode(), [&] (const auto& n) {
            const auto& val = n.value();
            if(val)
            {
                auto mess = message(State::Message{address(n), *val}, dl);
                if(mess)
                    elts.push_back(std::move(mess));
                else
                    qDebug( ) << State::Message{address(n), *val} << " message creation failed";
            }
    });

    for(auto& proc : iscore_state.stateProcesses)
    {
        auto fac = ctx.stateProcesses.factory(proc);
        if(fac)
        {
            auto state = fac->make(proc, ctx);
            if(state)
            {
                elts.push_back(state);
            }
        }
    }

    return ossia_state;
}


std::shared_ptr<OSSIA::State> state(
        const Scenario::StateModel& iscore_state,
        const RecreateOnPlay::Context& ctx)
{
    return state(OSSIA::State::create(), iscore_state, ctx);
}


static OSSIA::Destination* expressionAddress(
        const State::Address& addr,
        const Device::DeviceList& devlist)
{
    auto it = devlist.find(addr.device);
    if(it == devlist.devices().end())
        throw NodeNotFoundException(addr);

    auto& device = **it;
    if(!device.connected())
    {
        throw NodeNotFoundException(addr);
    }

    if(auto casted_dev = dynamic_cast<const Ossia::OSSIADevice*>(&device))
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
}

static OSSIA::Value* expressionOperand(
        const State::RelationMember& relm,
        const Device::DeviceList& list)
{
    using namespace eggs::variants;

    const struct {
        public:
            const Device::DeviceList& devlist;
            using return_type = OSSIA::Value*;
            return_type operator()(const State::Address& addr) const {
                return expressionAddress(addr, devlist);
            }

            return_type operator()(const State::Value& val) const {
                return toOSSIAValue(val).release();
            }

            return_type operator()(const State::AddressAccessor& acc) const {
                auto dest = expressionAddress(acc.address, devlist);
                if(dest)
                {
                    transform(acc.accessors, std::back_inserter(dest->index), [] (auto i) { return i;});
                }
                return dest;
            }
    } visitor{list};

    return eggs::variants::apply(visitor, relm);
}

static OSSIA::ExpressionAtom::Operator expressionOperator(State::Relation::Operator op)
{
    switch(op)
    {
        case State::Relation::Operator::Different:
            return OSSIA::ExpressionAtom::Operator::DIFFERENT;
        case State::Relation::Operator::Equal:
            return OSSIA::ExpressionAtom::Operator::EQUAL;
        case State::Relation::Operator::Greater:
            return OSSIA::ExpressionAtom::Operator::GREATER_THAN;
        case State::Relation::Operator::GreaterEqual:
            return OSSIA::ExpressionAtom::Operator::GREATER_THAN_OR_EQUAL;
        case State::Relation::Operator::Lower:
            return OSSIA::ExpressionAtom::Operator::LOWER_THAN;
        case State::Relation::Operator::LowerEqual:
            return OSSIA::ExpressionAtom::Operator::LOWER_THAN_OR_EQUAL;
        default:
            ISCORE_ABORT;
    }
}

// State::Relation -> OSSIA::ExpressionAtom
static std::shared_ptr<OSSIA::ExpressionAtom> expressionAtom(
        const State::Relation& rel,
        const Device::DeviceList& dev)
{
    using namespace eggs::variants;

    return OSSIA::ExpressionAtom::create(
                expressionOperand(rel.lhs, dev),
                expressionOperator(rel.op),
                expressionOperand(rel.rhs, dev));
}

static std::shared_ptr<OSSIA::ExpressionPulse> expressionPulse(
        const State::Pulse& rel,
        const Device::DeviceList& dev)
{
    using namespace eggs::variants;

    return OSSIA::ExpressionPulse::create(expressionAddress(rel.address, dev));
}

static const QMap<State::BinaryOperator, OSSIA::ExpressionComposition::Operator> comp_map
{
    {State::BinaryOperator::And, OSSIA::ExpressionComposition::Operator::AND},
    {State::BinaryOperator::Or, OSSIA::ExpressionComposition::Operator::OR},
    {State::BinaryOperator::Xor, OSSIA::ExpressionComposition::Operator::XOR}
};

std::shared_ptr<OSSIA::Expression> expression(
        const State::Expression& e,
        const Device::DeviceList& list)
{
    const struct {
            const State::Expression& expr;
            const Device::DeviceList& devlist;
            using return_type = std::shared_ptr<OSSIA::Expression>;
            return_type operator()(const State::Relation& rel) const
            {
                return expressionAtom(rel, devlist);
            }
            return_type operator()(const State::Pulse& rel) const
            {
                return expressionPulse(rel, devlist);
            }

            return_type operator()(const State::BinaryOperator rel) const
            {
                const auto& lhs = expr.childAt(0);
                const auto& rhs = expr.childAt(1);
                return OSSIA::ExpressionComposition::create(
                            expression(lhs, devlist),
                            comp_map[rel],
                            expression(rhs, devlist)
                            );
            }
            return_type operator()(const State::UnaryOperator) const
            {
                return OSSIA::ExpressionNot::create(expression(expr.childAt(0), devlist));
            }
            return_type operator()(const InvisibleRootNodeTag) const
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

    } visitor{e, list};

    return eggs::variants::apply(visitor, e.impl());
}

void removeOSSIAAddress(OSSIA::Node* n)
{
    n->removeAddress();
}

}
}
