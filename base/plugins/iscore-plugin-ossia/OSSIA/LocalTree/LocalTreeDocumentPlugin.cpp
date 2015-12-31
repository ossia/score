#include "LocalTreeDocumentPlugin.hpp"
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
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

#include "Scenario/ScenarioComponent.hpp"

ISCORE_METADATA_IMPL(Ossia::LocalTree::DocumentPlugin)
Ossia::LocalTree::DocumentPlugin::DocumentPlugin(
        std::shared_ptr<OSSIA::Device> localDev,
        iscore::Document& doc,
        QObject* parent):
    iscore::DocumentPluginModel{doc, "LocalTree::DocumentPlugin", parent},
    m_localDevice{localDev}
{
    auto scenar = dynamic_cast<ScenarioDocumentModel*>(
                      &m_context.document.model().modelDelegate());
    ISCORE_ASSERT(scenar);
    auto& cstr = scenar->baseScenario().constraint();
    cstr.components.add(new ConstraintComponent(
                            *m_localDevice,
                            Id<iscore::Component>{0},
                            cstr,
                            *this,
                            doc.context(),
                            this));

    //Ossia::LocalTree::ScenarioVisitor v;
    //v.visit(cstr, m_dev);

}
