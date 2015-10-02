#include "CSPDocumentPlugin.hpp"

#include <Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <core/document/DocumentModel.hpp>

CSPDocumentPlugin::CSPDocumentPlugin(iscore::DocumentModel &doc, QObject* parent):
    iscore::DocumentDelegatePluginModel{"CSPDocumentPlugin", parent}
{
    reload(doc);
}

void CSPDocumentPlugin::reload(iscore::DocumentModel& document)
{
    auto scenarioBase = document.findChild<BaseScenario*>("BaseScenario");
    m_cspScenario = new CSPScenario(*scenarioBase, scenarioBase);
}

CSPScenario* CSPDocumentPlugin::getScenario() const
{
    return m_cspScenario;
}
