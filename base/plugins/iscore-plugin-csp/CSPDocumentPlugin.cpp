#include "CSPDocumentPlugin.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

CSPDocumentPlugin::CSPDocumentPlugin(iscore::Document& doc, QObject* parent):
    iscore::DocumentPluginModel{doc, "CSPDocumentPlugin", parent}
{
    reload(doc.model());
}

void CSPDocumentPlugin::reload(iscore::DocumentModel& document)
{
    auto scenar = dynamic_cast<ScenarioDocumentModel*>(&document.modelDelegate());
    if(!scenar)
        return;

    auto& scenarioBase = scenar->baseScenario();
    m_cspScenario = new CSPScenario(scenarioBase, &scenarioBase);
}

CSPScenario* CSPDocumentPlugin::getScenario() const
{
    return m_cspScenario;
}
