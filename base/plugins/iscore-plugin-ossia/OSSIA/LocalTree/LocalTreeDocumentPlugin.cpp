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

#include "SetProperty.hpp"
#include "GetProperty.hpp"
#include "Property.hpp"

auto add_node(OSSIA::Node& n, const std::string& name)
{
    return *n.emplace(n.children().end(), name);
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



void OSSIA::LocalTree::ScenarioVisitor::visit(
        Scenario::ScenarioModel& scenario,
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
    add_getProperty<QString>(*parent, "name", &metadata,
                          &ModelMetadata::name,
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
    add_getProperty<::TimeValue>(*parent, "min", &constraint.duration,
                               &ConstraintDurations::minDuration,
                               &ConstraintDurations::minDurationChanged
                               );
    add_getProperty<::TimeValue>(*parent, "max", &constraint.duration,
                               &ConstraintDurations::maxDuration,
                               &ConstraintDurations::maxDurationChanged
                               );
    add_getProperty<::TimeValue>(*parent, "default", &constraint.duration,
                               &ConstraintDurations::defaultDuration,
                               &ConstraintDurations::defaultDurationChanged
                               );
    add_getProperty<float>(*parent, "play", &constraint.duration,
                           &ConstraintDurations::playPercentage,
                           &ConstraintDurations::playPercentageChanged
                           );

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
    add_setProperty<impulse_t>(*parent, "trigger", tn.trigger(),
                 [&] (auto) {
        tn.trigger()->triggered();
    });
}



void OSSIA::LocalTree::ScenarioVisitor::visit(
        StateModel& state,
        const std::shared_ptr<OSSIA::Node>& parent)
{

}

