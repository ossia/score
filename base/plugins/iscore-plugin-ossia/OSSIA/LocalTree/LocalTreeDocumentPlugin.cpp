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

class PropertyWrapper : public QObject
{
    public:
        std::shared_ptr<OSSIA::Node> m_node;
        std::shared_ptr<OSSIA::Address> m_addr;

        OSSIA::Address::iterator m_cb{};

        template<typename T, typename Object, typename PropGet, typename PropSet, typename PropChanged>
        PropertyWrapper(
                const std::shared_ptr<OSSIA::Node>& node,
                const std::shared_ptr<OSSIA::Address>& addr,
                Object* parent,
                PropGet get,
                PropSet set,
                PropChanged chgd,
                T deflt):
            QObject{parent},
            m_node{node},
            m_addr{addr}
        {
            m_cb = m_addr->addCallback([=] (const OSSIA::Value* v) {
                if(v)
                    (parent->*set)(iscore::convert::value<T>(OSSIA::convert::ToValue(v)));
            });

            connect(parent, chgd,
                    this, [=] {
                m_addr->pushValue(iscore::convert::toOSSIAValue(iscore::Value::fromValue((parent->*get)())));
            });

        }

        void val(OSSIA::Value* val)
        {
            qDebug() << val;
            qDebug() << (int)val->getType();

        }

        ~PropertyWrapper()
        {
            m_addr->removeCallback(m_cb);
        }
};

template<typename Prop>
auto matchingType()
{
    return OSSIA::Value::Type::FLOAT;
}

template<typename T, typename Object, typename PropGet, typename PropSet, typename PropChanged>
auto add_property(OSSIA::Node& n, const std::string& name, Object* obj, PropGet get, PropSet set, PropChanged chgd)
{
    std::shared_ptr<OSSIA::Node> node = *n.emplace(n.children().end(), name);
    auto addr = node->createAddress(matchingType<T>());

    return new PropertyWrapper{node, addr, obj, get, set, chgd, T{}};
}

namespace OSSIA
{
namespace LocalTree
{

struct ScenarioVisitor
{
        void visit(Scenario::ScenarioModel& scenario,
                   const std::shared_ptr<OSSIA::Node>& parent)
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
                visit(elt, timenodes);
            }

            auto states = add_node(*parent, "states");
            for(auto& elt : scenario.states)
            {
                visit(elt, states);
            }
        }

        void visit(ModelMetadata& metadata,
                   const std::shared_ptr<OSSIA::Node>& parent)
        {
            auto it = add_node(*parent, metadata.comment().toStdString());

        }

        void visit(ConstraintModel& constraint,
                   const std::shared_ptr<OSSIA::Node>& parent)
        {
            add_property<float>(*parent, "yPos", &constraint,
                         &ConstraintModel::heightPercentage,
                         &ConstraintModel::setHeightPercentage,
                         &ConstraintModel::heightPercentageChanged);
        }

        void visit(EventModel& ev,
                   const std::shared_ptr<OSSIA::Node>& parent)
        {

        }

        void visit(TimeNodeModel& tn,
                   const std::shared_ptr<OSSIA::Node>& parent)
        {

        }

        void visit(StateModel& state,
                   const std::shared_ptr<OSSIA::Node>& parent)
        {

        }
};

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

}
}
