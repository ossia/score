#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <core/document/DocumentModel.hpp>

#include "BaseScenarioElement.hpp"
#include "DocumentPlugin.hpp"
#include "iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp"

class QObject;
namespace iscore {
class Document;
}  // namespace iscore


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
