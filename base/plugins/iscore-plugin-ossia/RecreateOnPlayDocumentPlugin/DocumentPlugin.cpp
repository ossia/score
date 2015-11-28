#include "DocumentPlugin.hpp"
#include "AutomationElement.hpp"

#include "ConstraintElement.hpp"
#include "EventElement.hpp"
#include "TimeNodeElement.hpp"

#include "BaseScenarioElement.hpp"
#include "ScenarioElement.hpp"

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Automation/AutomationModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>


namespace RecreateOnPlay
{
DocumentPlugin::DocumentPlugin(iscore::Document& doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{doc, "OSSIADocumentPlugin", parent}
{
}

void DocumentPlugin::reload(iscore::DocumentModel &doc)
{
    auto& baseElement = static_cast<ScenarioDocumentModel&>(doc.modelDelegate()).baseScenario();
    m_base = new BaseScenarioElement{baseElement, this};
}

void DocumentPlugin::clear()
{
    delete m_base;
    m_base = nullptr;
}

BaseScenarioElement *DocumentPlugin::baseScenario() const
{
    return m_base;
}
}
