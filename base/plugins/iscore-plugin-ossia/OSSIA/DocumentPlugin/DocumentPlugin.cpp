#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include "BaseScenarioElement.hpp"
#include "DocumentPlugin.hpp"
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <core/document/DocumentModel.hpp>

class QObject;
namespace iscore {
class Document;
}  // namespace iscore


namespace RecreateOnPlay
{
DocumentPlugin::DocumentPlugin(iscore::Document& doc, QObject* parent):
    iscore::DocumentPluginModel{doc, "OSSIADocumentPlugin", parent}
{
}

void DocumentPlugin::reload(BaseScenario& doc)
{
    // TODO unique_ptr and stop it.
    m_base = new BaseScenarioElement{doc, this};
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
