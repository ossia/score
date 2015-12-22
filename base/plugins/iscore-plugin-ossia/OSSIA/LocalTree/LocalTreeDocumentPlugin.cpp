#include "LocalTreeDocumentPlugin.hpp"
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <Network/Device.h>
#include <Editor/Value.h>
#include <Network/Address.h>


#include <Curve/CurveModel.hpp>
#include <Automation/AutomationModel.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <State/Message.hpp>
#include <State/Value.hpp>

#include <Process/State/MessageNode.hpp>

#include <Curve/Segment/CurveSegmentData.hpp>

#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
auto add_node(OSSIA::Node& n, const std::string& name)
{
    return *n.emplace(n.children().end(), name);
}

template<
        typename T,
        typename Object,
        typename PropGet,
        typename PropSet,
        typename PropChanged
>
class QtProperty
{
        Object& m_obj;
        PropGet m_get{};
        PropSet m_set{};
        PropChanged m_changed{};

    public:
        QtProperty(
                Object& obj,
                PropGet get,
                PropSet set,
                PropChanged chgd):
            m_obj{obj},
            m_get{get},
            m_set{set},
            m_changed{chgd}
        {

        }

        auto get() const
        { return (m_obj.*m_get)(); }

        auto set(const T& newval) const
        { return (m_obj.*m_set)(newval); }
        auto set(const iscore::Value& newval) const
        { return (m_obj.*m_set)(iscore::convert::value<T>(newval)); }

        auto changed() const
        { return (m_obj.*m_changed); }

        auto& object() const
        { return m_obj; }
        auto changed_property() const
        { return m_changed; }
};

/*

template<
        typename Object,
        typename Callback
>
class QtCallback
{
        Object& m_obj;
        Callback m_callback{};

    public:
        QtCallback(
                Object& obj,
                Callback cb):
            m_obj{obj},
            m_callback{cb}
        {

        }

        auto& object() const
        { return m_obj; }
        auto callback() const
        { return m_changed; }
};
*/

template<typename Callback>
class CallbackWrapper : public QObject
{
    public:
        std::shared_ptr<OSSIA::Node> m_node;
        std::shared_ptr<OSSIA::Address> m_addr;
        Callback m_callback;

        OSSIA::Address::iterator m_cb{};

        CallbackWrapper(
                const std::shared_ptr<OSSIA::Node>& node,
                const std::shared_ptr<OSSIA::Address>& addr,
                Callback prop,
                QObject* parent
                ):
            QObject{parent},
            m_node{node},
            m_addr{addr},
            m_callback{prop}
        {
            m_cb = m_addr->addCallback([=] (const OSSIA::Value* v) {
                m_callback();
            });

            addr->pushValue(new OSSIA::Impulse);
        }

        ~CallbackWrapper()
        {
            m_addr->removeCallback(m_cb);
        }
};

template<typename Callback>
auto make_callback(
        const std::shared_ptr<OSSIA::Node>& node,
        const std::shared_ptr<OSSIA::Address>& addr,
        Callback prop,
        QObject* parent)
{
    return new CallbackWrapper<Callback>{node, addr, prop, parent};
}

template<typename Property>
class PropertyWrapper : public QObject
{
    public:
        std::shared_ptr<OSSIA::Node> m_node;
        std::shared_ptr<OSSIA::Address> m_addr;
        Property m_property;

        OSSIA::Address::iterator m_cb{};

        PropertyWrapper(
                const std::shared_ptr<OSSIA::Node>& node,
                const std::shared_ptr<OSSIA::Address>& addr,
                Property prop,
                QObject* parent
                ):
            QObject{parent},
            m_node{node},
            m_addr{addr},
            m_property{prop}
        {
            m_cb = m_addr->addCallback([=] (const OSSIA::Value* v) {
                if(v)
                    m_property.set(OSSIA::convert::ToValue(v));
            });

            connect(&m_property.object(), m_property.changed_property(),
                    this, [=] {
                auto newVal = iscore::Value::fromValue(m_property.get());
                if(newVal != OSSIA::convert::ToValue(m_addr->getValue()))
                    m_addr->pushValue(iscore::convert::toOSSIAValue(newVal));
            });

            addr->pushValue(
                            iscore::convert::toOSSIAValue(
                                iscore::Value::fromValue(m_property.get())));
        }

        ~PropertyWrapper()
        {
            m_addr->removeCallback(m_cb);
        }
};

template<typename Property>
auto make_property(
        const std::shared_ptr<OSSIA::Node>& node,
        const std::shared_ptr<OSSIA::Address>& addr,
        Property prop,
        QObject* parent)
{
    return new PropertyWrapper<Property>{node, addr, prop, parent};
}

template<typename Prop> auto matchingType();

template<> auto matchingType<float>()       { return OSSIA::Value::Type::FLOAT; }
template<> auto matchingType<double>()      { return OSSIA::Value::Type::FLOAT; }
template<> auto matchingType<int>()         { return OSSIA::Value::Type::INT; }
template<> auto matchingType<bool>()        { return OSSIA::Value::Type::BOOL; }
template<> auto matchingType<impulse_t>()   { return OSSIA::Value::Type::IMPULSE; }
template<> auto matchingType<std::string>() { return OSSIA::Value::Type::STRING; }
template<> auto matchingType<QString>()     { return OSSIA::Value::Type::STRING; }
template<> auto matchingType<char>()        { return OSSIA::Value::Type::CHAR; }
template<> auto matchingType<QChar>()       { return OSSIA::Value::Type::CHAR; }
template<> auto matchingType<tuple_t>()     { return OSSIA::Value::Type::TUPLE; }

template<typename T, typename Object, typename PropGet, typename PropSet, typename PropChanged>
auto add_property(OSSIA::Node& n, const std::string& name, Object* obj, PropGet get, PropSet set, PropChanged chgd)
{
    std::shared_ptr<OSSIA::Node> node = *n.emplace(n.children().end(), name);
    auto addr = node->createAddress(matchingType<T>());

    return make_property(node, addr, QtProperty<T, Object, PropGet, PropSet, PropChanged>{*obj, get, set, chgd}, obj);
}

template<typename Object, typename Callback>
auto add_callback(OSSIA::Node& n, const std::string& name, Object* obj, Callback cb)
{
    std::shared_ptr<OSSIA::Node> node = *n.emplace(n.children().end(), name);
    auto addr = node->createAddress(matchingType<impulse_t>());

    return make_callback(node, addr, cb, obj);
}

class DocumentPlugin : public iscore::DocumentPluginModel
{
        std::shared_ptr<OSSIA::Device> m_localDevice;
    public:
        DocumentPlugin(
                std::shared_ptr<OSSIA::Device> localDev,
                iscore::Document& doc,
                QObject* parent):
            iscore::DocumentPluginModel{doc, "LocalTreeDocumentPlugin", parent},
            m_localDevice{localDev}
        {

        }
};



void OSSIA::LocalTree::ScenarioVisitor::visit(Scenario::ScenarioModel& scenario, const std::shared_ptr<OSSIA::Node>& parent)
{
    auto constraints = add_node(*parent, "constraints");
    for(ConstraintModel& elt : scenario.constraints)
    {
        auto it = add_node(*constraints, elt.metadata.name().toStdString());
        visit(elt, it);
    }

    auto events = add_node(*parent, "events");
    for(auto& elt : scenario.events)
    {
        visit(elt, events);
    }

    auto timenodes = add_node(*parent, "timenodes");
    for(auto& elt : scenario.timeNodes)
    {
        auto it = add_node(*timenodes, elt.metadata.name().toStdString());
        visit(elt, it);
    }

    auto states = add_node(*parent, "states");
    for(auto& elt : scenario.states)
    {
        visit(elt, states);
    }
}



void OSSIA::LocalTree::ScenarioVisitor::visit(
        ModelMetadata& metadata,
        const std::shared_ptr<OSSIA::Node>& parent)
{
    add_property<QString>(*parent, "name", &metadata,
                          &ModelMetadata::name,
                          &ModelMetadata::setName,
                          &ModelMetadata::nameChanged);
    add_property<QString>(*parent, "comment", &metadata,
                          &ModelMetadata::comment,
                          &ModelMetadata::setComment,
                          &ModelMetadata::commentChanged);
    add_property<QString>(*parent, "label", &metadata,
                          &ModelMetadata::label,
                          &ModelMetadata::setLabel,
                          &ModelMetadata::labelChanged);
}

void OSSIA::LocalTree::ScenarioVisitor::visit(
        ConstraintModel& constraint,
        const std::shared_ptr<OSSIA::Node>& parent)
{
    add_property<float>(*parent, "yPos", &constraint,
                        &ConstraintModel::heightPercentage,
                        &ConstraintModel::setHeightPercentage,
                        &ConstraintModel::heightPercentageChanged);


    auto processes = add_node(*parent, "processes");
    for(Process& proc : constraint.processes)
    {
        auto it = add_node(*processes, proc.metadata.name().toStdString());
        if(auto scenario = dynamic_cast<Scenario::ScenarioModel*>(&proc))
        {
            visit(*scenario, it);
        }
    }
}



void OSSIA::LocalTree::ScenarioVisitor::visit(
        EventModel& ev,
        const std::shared_ptr<OSSIA::Node>& parent)
{

}



void OSSIA::LocalTree::ScenarioVisitor::visit(
        TimeNodeModel& tn,
        const std::shared_ptr<OSSIA::Node>& parent)
{
    add_callback(*parent, "trigger", tn.trigger(),
                 [&] () {
        tn.trigger()->triggered();
    });
}



void OSSIA::LocalTree::ScenarioVisitor::visit(
        StateModel& state,
        const std::shared_ptr<OSSIA::Node>& parent)
{

}

