#include "LocalTreeDocumentPlugin.hpp"
#include <ossia/network/base/device.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/address.hpp>


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
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

#include "Scenario/ScenarioComponent.hpp"
#include <OSSIA/LocalTree/Settings/LocalTreeModel.hpp>

Ossia::LocalTree::DocumentPlugin::DocumentPlugin(
        iscore::Document& doc,
        QObject* parent):
    iscore::DocumentPlugin{doc.context(), "LocalTree::DocumentPlugin", parent},
    m_localDevice{std::make_unique<ossia::net::local_protocol>(), "i-score"}
{
    con(doc, &iscore::Document::aboutToClose,
        this, &DocumentPlugin::cleanup);

    auto& set = m_context.app.settings<Settings::Model>();
    if(set.getLocalTree())
    {
        create();
    }

    con(set, &Settings::Model::LocalTreeChanged,
        this, [=] (bool b) {
        if(b)
            create();
        else
            cleanup();
    }, Qt::QueuedConnection);
}

Ossia::LocalTree::DocumentPlugin::~DocumentPlugin()
{
    cleanup();
}

void Ossia::LocalTree::DocumentPlugin::create()
{
    if(m_root)
        cleanup();

    auto& doc = m_context.document.model().modelDelegate();
    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
                      &doc);
    ISCORE_ASSERT(scenar);
    auto& cstr = scenar->baseScenario().constraint();
    m_root = new Constraint(
                m_localDevice.getRootNode(),
                getStrongId(cstr.components),
                cstr,
                *this,
                this);
    cstr.components.add(m_root);
}

void Ossia::LocalTree::DocumentPlugin::cleanup()
{
    if(!m_root)
        return;

    // Remove the node from local device
    //m_localDevice.getRootNode().removeChild(m_root->node());

    // Delete
    // TODO why not delete m_root;
    m_root = nullptr;
    /*
    auto& doc = m_context.document.model().modelDelegate();
    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
                      &doc);
    ISCORE_ASSERT(scenar);
    auto& cstr = scenar->baseScenario().constraint();
    m_root = nullptr;
    auto cit = cstr.components.find(m_root->id());
    if(cit != cstr.components.end())
        cstr.components.remove(m_root);
    m_root = nullptr;
    */
}
