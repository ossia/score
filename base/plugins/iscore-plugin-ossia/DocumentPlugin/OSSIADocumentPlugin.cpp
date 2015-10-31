#include "OSSIADocumentPlugin.hpp"
#include "OSSIAAutomationElement.hpp"

#include "OSSIAConstraintElement.hpp"
#include "OSSIAEventElement.hpp"
#include "OSSIATimeNodeElement.hpp"

#include "OSSIABaseScenarioElement.hpp"
#include "OSSIAScenarioElement.hpp"

#include "Document/BaseElement/BaseScenario/BaseScenario.hpp"
#include <Process/ScenarioModel.hpp>
#include <core/document/DocumentModel.hpp>
#include <Automation/AutomationModel.hpp>


OSSIADocumentPlugin::OSSIADocumentPlugin(iscore::DocumentModel &doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{"OSSIADocumentPlugin", parent}
{
    reload(doc);
}

void OSSIADocumentPlugin::reload(iscore::DocumentModel &doc)
{
    auto baseElement = doc.findChild<BaseScenario*>("BaseScenario");
    m_base = new OSSIABaseScenarioElement{baseElement, this};
}

OSSIABaseScenarioElement *OSSIADocumentPlugin::baseScenario() const
{
    return m_base;
}
