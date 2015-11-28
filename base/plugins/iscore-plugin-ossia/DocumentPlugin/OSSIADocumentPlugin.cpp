#include "OSSIADocumentPlugin.hpp"
#include "OSSIAAutomationElement.hpp"

#include "OSSIAEventElement.hpp"

#include "OSSIABaseScenarioElement.hpp"
#include "OSSIAScenarioElement.hpp"

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>


OSSIADocumentPlugin::OSSIADocumentPlugin(iscore::Document& doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{doc, "OSSIADocumentPlugin", parent}
{
    //reload(doc.model());
}

void OSSIADocumentPlugin::reload(iscore::DocumentModel &doc)
{
    auto& baseElement = static_cast<ScenarioDocumentModel&>(doc.modelDelegate()).baseScenario();
    m_base = new OSSIABaseScenarioElement{baseElement, this};
}

void OSSIADocumentPlugin::clear()
{
    delete m_base;
    m_base = nullptr;
}

OSSIABaseScenarioElement *OSSIADocumentPlugin::baseScenario() const
{
    return m_base;
}
