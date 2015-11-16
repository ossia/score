#include "OSSIADocumentPlugin.hpp"
#include "OSSIAAutomationElement.hpp"

#include "OSSIAConstraintElement.hpp"
#include "OSSIAEventElement.hpp"
#include "OSSIATimeNodeElement.hpp"

#include "OSSIABaseScenarioElement.hpp"
#include "OSSIAScenarioElement.hpp"

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>
#include <Automation/AutomationModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>


OSSIADocumentPlugin::OSSIADocumentPlugin(iscore::Document& doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{doc.context(), "OSSIADocumentPlugin", parent}
{
    reload(doc.model());
}

void OSSIADocumentPlugin::reload(iscore::DocumentModel &doc)
{
    auto& baseElement = static_cast<BaseElementModel&>(*doc.modelDelegate()).baseScenario();
    m_base = new OSSIABaseScenarioElement{baseElement, this};
}

OSSIABaseScenarioElement *OSSIADocumentPlugin::baseScenario() const
{
    return m_base;
}
