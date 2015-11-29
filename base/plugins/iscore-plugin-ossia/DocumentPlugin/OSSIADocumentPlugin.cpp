#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <core/document/DocumentModel.hpp>

#include "OSSIABaseScenarioElement.hpp"
#include "OSSIADocumentPlugin.hpp"
#include "iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp"

class QObject;
namespace iscore {
class Document;
}  // namespace iscore


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
