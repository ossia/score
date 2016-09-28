#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/expression/expression_atom.hpp>
#include <ossia/editor/expression/expression_composition.hpp>
#include <ossia/editor/expression/expression_not.hpp>
#include <ossia/editor/expression/expression_pulse.hpp>
#include <Process/State/MessageNode.hpp>
#include <boost/concept/usage.hpp>

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
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <State/Address.hpp>
#include <State/Expression.hpp>
#include <State/Message.hpp>
#include <State/Relation.hpp>
#include <iscore/tools/InvisibleRootNode.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/StateProcessComponent.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Engine/Executor/ProcessElement.hpp>

#include <boost/call_traits.hpp>
class NodeNotFoundException : public std::runtime_error
{
        State::Address m_addr;
    public:
        NodeNotFoundException(const State::Address& n):
            std::runtime_error{QString("Address: %1 not found in actual tree.").arg(m_addr.toString()).toStdString()},
            m_addr(n)
        {

        }
};

namespace Engine
{
namespace iscore_to_ossia
{

ossia::net::node_base *createNodeFromPath(
        const QStringList &path,
        ossia::net::device_base& dev)
{
    using namespace ossia;
    // Find the relevant node to add in the device
    ossia::net::node_base* node = &dev.getRootNode();
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = ::find_if(children,
                            [&] (const auto& ossia_node) { return ossia_node->getName() == path[i].toStdString(); });

        if(it == children.end())
        {
            // We have to start adding sub-nodes from here.
            ossia::net::node_base* parentnode = node;
            for(int k = i; k < path.size(); k++)
            {
                auto newNode = parentnode->createChild(path[k].toStdString());
                if(path[k].toStdString() != newNode->getName())
                {
                    qDebug() << path[k] << newNode->getName().c_str();
                    for(const auto& node : parentnode->children())
                    {
                        qDebug() << node->getName().c_str();
                    }
                    ISCORE_ABORT;
                }
                if(k == path.size() - 1)
                {
                    node = newNode;
                }
                else
                {
                    parentnode = newNode;
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

ossia::net::node_base* findNodeFromPath(
        const QStringList& path, ossia::net::device_base& dev)
{
    using namespace ossia;
    // Find the relevant node to add in the device
    ossia::net::node_base* node = &dev.getRootNode();
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();
        auto it = ::find_if(children,
                          [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });
        if(it != children.end())
            node = it->get();
        else
        {
            qDebug() << "looking for" << path << " -- last found: " << node->getName() << "\n";
            return {};
        }
    }

    return node;
}

ossia::net::node_base* getNodeFromPath(
        const QStringList &path,
        ossia::net::device_base& dev)
{
    using namespace ossia;
    // Find the relevant node to add in the device
    ossia::net::node_base* node = &dev.getRootNode();
    for(int i = 0; i < path.size(); i++)
    {
        const auto& children = node->children();

        auto it = ::find_if(children, [&] (const auto& ossia_node)
        {
            return ossia_node->getName() == path[i].toStdString();
        });

        ISCORE_ASSERT(it != children.end());

        node = it->get();
    }

    ISCORE_ASSERT(node);
    return node;
}

static ossia::bounding_mode toBoundingMode(Device::ClipMode c)
{
    switch(c)
    {
        case Device::ClipMode::Free:
            return ossia::bounding_mode::FREE;
        case Device::ClipMode::Clip:
            return ossia::bounding_mode::CLIP;
        case Device::ClipMode::Wrap:
            return ossia::bounding_mode::WRAP;
        case Device::ClipMode::Fold:
            return ossia::bounding_mode::FOLD;
        case Device::ClipMode::Low:
            return ossia::bounding_mode::LOW;
        case Device::ClipMode::High:
            return ossia::bounding_mode::HIGH;
        default:
            ISCORE_ABORT;
            return static_cast<ossia::bounding_mode>(-1);
    }
}


void updateOSSIAAddress(
        const Device::FullAddressSettings &settings,
        ossia::net::address_base& addr)
{
    switch(settings.ioType)
    {
        case Device::IOType::In:
            addr.setAccessMode(ossia::access_mode::GET);
            break;
        case Device::IOType::Out:
            addr.setAccessMode(ossia::access_mode::SET);
            break;
        case Device::IOType::InOut:
            addr.setAccessMode(ossia::access_mode::BI);
            break;
        case Device::IOType::Invalid:
            ISCORE_ABORT;
            break;
    }

    addr.setRepetitionFilter(ossia::repetition_filter(settings.repetitionFilter));
    addr.setBoundingMode(Engine::iscore_to_ossia::toBoundingMode(settings.clipMode));

    addr.setValue(Engine::iscore_to_ossia::toOSSIAValue(settings.value));

    addr.setDomain(
                ossia::net::make_domain(
                    toOSSIAValue(settings.domain.min),
                    toOSSIAValue(settings.domain.max)));

}

void createOSSIAAddress(
        const Device::FullAddressSettings &settings,
        ossia::net::node_base& node)
{
    if(settings.value.val.is<State::no_value_t>())
        return;

    struct {
        public:
            using return_type = ossia::val_type;
            return_type operator()(const State::no_value_t&) const { ISCORE_ABORT; return ossia::val_type::IMPULSE; }
            return_type operator()(const State::impulse_t&) const { return ossia::val_type::IMPULSE; }
            return_type operator()(int) const { return ossia::val_type::INT; }
            return_type operator()(float) const { return ossia::val_type::FLOAT; }
            return_type operator()(bool) const { return ossia::val_type::BOOL; }
            return_type operator()(const QString&) const { return ossia::val_type::STRING; }
            return_type operator()(QChar) const { return ossia::val_type::CHAR; }
            return_type operator()(const State::vec2f&) const { return ossia::val_type::VEC2F; }
            return_type operator()(const State::vec3f&) const { return ossia::val_type::VEC3F; }
            return_type operator()(const State::vec4f&) const { return ossia::val_type::VEC4F; }
            return_type operator()(const State::tuple_t&) const { return ossia::val_type::TUPLE; }
    } visitor{};

    auto addr = node.createAddress(eggs::variants::apply(visitor, settings.value.val.impl()));
    if(addr)
        updateOSSIAAddress(settings, *addr);
}

void updateOSSIAValue(const State::ValueImpl& iscore_data, ossia::value& val)
{
    struct {
            const State::ValueImpl& data;
            void operator()(const ossia::Destination&) const { }
            void operator()(const ossia::Behavior&) const { }
            void operator()(ossia::Impulse) const { }
            void operator()(ossia::Int& v) const { v = data.get<int>(); }
            void operator()(ossia::Float& v) const { v = data.get<float>(); }
            void operator()(ossia::Bool& v) const { v = data.get<bool>(); }
            void operator()(ossia::Char& v) const { v = data.get<QChar>().toLatin1(); }
            void operator()(ossia::String& v) const { v = data.get<QString>().toStdString(); }
            void operator()(ossia::Vec2f& v) const { v.value = data.get<State::vec2f>(); }
            void operator()(ossia::Vec3f& v) const { v.value = data.get<State::vec3f>(); }
            void operator()(ossia::Vec4f& v) const { v.value = data.get<State::vec4f>(); }
            void operator()(ossia::Tuple& v) const
            {
                State::tuple_t tuple = data.get<State::tuple_t>();
                auto& vec = v.value;
                ISCORE_ASSERT(tuple.size() == vec.size());
                int n = vec.size();
                for(int i = 0; i < n; i++)
                {
                    updateOSSIAValue(tuple[i], vec[i]);
                }
            }
    } visitor{iscore_data};

    return eggs::variants::apply(visitor, val.v);
}

static ossia::value toOSSIAValue(const State::ValueImpl& val)
{
    struct {
            using return_type = ossia::value;
            return_type operator()(const State::no_value_t&) const { return ossia::value{}; }
            return_type operator()(const State::impulse_t&) const { return ossia::Impulse{}; }
            return_type operator()(int v) const { return ossia::Int{v}; }
            return_type operator()(float v) const { return ossia::Float{v}; }
            return_type operator()(bool v) const { return ossia::Bool{v}; }
            return_type operator()(const QString& v) const { return ossia::String{v.toStdString()}; }
            return_type operator()(QChar v) const { return ossia::Char{v.toLatin1()}; }
            return_type operator()(const State::vec2f& v) const { return ossia::Vec2f{v}; }
            return_type operator()(const State::vec3f& v) const { return ossia::Vec3f{v}; }
            return_type operator()(const State::vec4f& v) const { return ossia::Vec4f{v}; }
            return_type operator()(const State::tuple_t& v) const
            {
                ossia::Tuple ossia_tuple;
                ossia_tuple.value.reserve(v.size());
                for(const auto& tuple_elt : v)
                {
                    ossia_tuple.value.push_back(eggs::variants::apply(*this, tuple_elt.impl()));
                }

                return ossia_tuple;
            }
    } visitor{};

    return eggs::variants::apply(visitor, val.impl());
}

ossia::value toOSSIAValue(
        const State::Value& value)
{
    return toOSSIAValue(value.val);
}

optional<ossia::message> message(
        const State::Message& mess,
        const Device::DeviceList& deviceList)
{
    auto dev_p = deviceList.findDevice(mess.address.address.device);
    if(!dev_p)
        return {};

    // OPTIMIZEME by sorting by device prior
    // to this.
    const auto& dev = *dev_p;
    if(!dev.connected())
        return {};

    if(auto casted_dev = dynamic_cast<const Engine::Network::OSSIADevice*>(&dev))
    {
        auto dev = casted_dev->getDevice();
        if(!dev)
            return {};

        auto ossia_node = Engine::iscore_to_ossia::findNodeFromPath(
                    mess.address.address.path,
                    *dev);

        if(!ossia_node)
            return {};
        auto ossia_addr = ossia_node->getAddress();
        if (!ossia_addr)
            return {};

        auto val = Engine::iscore_to_ossia::toOSSIAValue(mess.value);
        if(!val.valid())
            return {};

        return ossia::message{*ossia_addr, std::move(val)};
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

void state(
        ossia::state& parent,
        const Scenario::StateModel& iscore_state,
        const Engine::Execution::Context& ctx
        )
{
    auto& elts = parent;

    // For all elements where IOType != Invalid,
    // we add the elements to the state.

    auto& dl = ctx.devices.list();
    visit_node(iscore_state.messages().rootNode(), [&] (const auto& n) {
        const auto& val = n.value();
        if(val)
        {
            elts.add(message(State::Message{address(n), *val}, dl));
        }
    });

    for(auto& proc : iscore_state.stateProcesses)
    {
        auto fac = ctx.stateProcesses.factory(proc);
        if(fac)
        {
            elts.add(fac->make(proc, ctx));
        }
    }
}


ossia::state state(
        const Scenario::StateModel& iscore_state,
        const Engine::Execution::Context& ctx)
{
    ossia::state s;
    Engine::iscore_to_ossia::state(s, iscore_state, ctx);
    return s;
}


static ossia::Destination expressionAddress(
        const State::Address& addr,
        const Device::DeviceList& devlist)
{
    auto dev_p = devlist.findDevice(addr.device);
    if(!dev_p)
        throw NodeNotFoundException(addr);

    auto& device = *dev_p;
    if(!device.connected())
    {
        throw NodeNotFoundException(addr);
    }

    if(auto casted_dev = dynamic_cast<const Engine::Network::OSSIADevice*>(&device))
    {
        auto dev = casted_dev->getDevice();
        if(!dev)
            throw NodeNotFoundException(addr);

        auto n = findNodeFromPath(addr.path, *dev);
        if(n)
        {
            auto ossia_addr = n->getAddress();
            if(ossia_addr)
                return ossia::Destination(*ossia_addr);
            else
                throw NodeNotFoundException(addr);
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

static ossia::value expressionOperand(
        const State::RelationMember& relm,
        const Device::DeviceList& list)
{
    using namespace eggs::variants;

    const struct {
        public:
            const Device::DeviceList& devlist;
            using return_type = ossia::value;
            return_type operator()(const State::Address& addr) const {
                return expressionAddress(addr, devlist);
            }

            return_type operator()(const State::Value& val) const {
                return toOSSIAValue(val);
            }

            return_type operator()(const State::AddressAccessor& acc) const {
                auto dest = expressionAddress(acc.address, devlist);

                transform(acc.accessors, std::back_inserter(dest.index), [] (auto i) { return i;});

                return dest;
            }
    } visitor{list};

    return eggs::variants::apply(visitor, relm);
}

static ossia::expressions::expression_atom::Comparator
expressionComparator(State::Relation::Comparator op)
{
    using namespace ossia::expressions;
    switch(op)
    {
        case State::Relation::Comparator::Different:
            return expression_atom::Comparator::DIFFERENT;
        case State::Relation::Comparator::Equal:
            return expression_atom::Comparator::EQUAL;
        case State::Relation::Comparator::Greater:
            return expression_atom::Comparator::GREATER_THAN;
        case State::Relation::Comparator::GreaterEqual:
            return expression_atom::Comparator::GREATER_THAN_OR_EQUAL;
        case State::Relation::Comparator::Lower:
            return expression_atom::Comparator::LOWER_THAN;
        case State::Relation::Comparator::LowerEqual:
            return expression_atom::Comparator::LOWER_THAN_OR_EQUAL;
        default:
            ISCORE_ABORT;
    }
}

static ossia::expressions::expression_composition::Operator
expressionOperator(State::BinaryOperator op)
{
    using namespace ossia::expressions;
    switch(op)
    {
        case State::BinaryOperator::And:
            return expression_composition::Operator::AND;
        case State::BinaryOperator::Or:
            return expression_composition::Operator::OR;
        case State::BinaryOperator::Xor:
            return expression_composition::Operator::XOR;
        default:
            ISCORE_ABORT;
    }
}
// State::Relation -> OSSIA::ExpressionAtom
static ossia::expression_ptr expressionAtom(
        const State::Relation& rel,
        const Device::DeviceList& dev)
{
    using namespace eggs::variants;

    return ossia::expressions::make_expression_atom(
                expressionOperand(rel.lhs, dev),
                expressionComparator(rel.op),
                expressionOperand(rel.rhs, dev));
}

static ossia::expression_ptr expressionPulse(
        const State::Pulse& rel,
        const Device::DeviceList& dev)
{
    using namespace eggs::variants;

    return ossia::expressions::make_expression_pulse(expressionAddress(rel.address, dev));
}

ossia::expression_ptr expression(
        const State::Expression& e,
        const Device::DeviceList& list)
{
    const struct {
            const State::Expression& expr;
            const Device::DeviceList& devlist;
            using return_type = ossia::expression_ptr;
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
                return ossia::expressions::make_expression_composition(
                            expression(lhs, devlist),
                            expressionOperator(rel),
                            expression(rhs, devlist)
                            );
            }
            return_type operator()(const State::UnaryOperator) const
            {
                return ossia::expressions::make_expression_not(
                            expression(expr.childAt(0), devlist));
            }
            return_type operator()(const InvisibleRootNodeTag) const
            {
                if(expr.childCount() == 0)
                {
                    // By default no expression == true
                    return ossia::expressions::make_expression_true();
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

ossia::net::address_base* findAddress(
        const Device::DeviceList& devs,
        const State::Address& addr)
{
    auto dev_p =
            dynamic_cast<Engine::Network::OSSIADevice*>(
                devs.findDevice(addr.device));
    if(dev_p)
    {
        auto ossia_dev = dev_p->getDevice();
        if(ossia_dev)
        {
            auto node = Engine::iscore_to_ossia::findNodeFromPath(addr.path, *ossia_dev);
            if(node)
                return node->getAddress();
        }
    }
    return {};
}

}
}
