#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include "BaseScenarioElement.hpp"
#include "DocumentPlugin.hpp"
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <core/document/DocumentModel.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
class QObject;
namespace iscore {
class Document;
}  // namespace iscore

ISCORE_METADATA_IMPL(RecreateOnPlay::DocumentPlugin)
namespace RecreateOnPlay
{
DocumentPlugin::DocumentPlugin(iscore::Document& doc, QObject* parent):
    iscore::DocumentPluginModel{doc, "OSSIADocumentPlugin", parent}
{
}

DocumentPlugin::~DocumentPlugin()
{
    if(m_base)
    {
        m_base->baseConstraint()->stop();
    }
}

void DocumentPlugin::reload(BaseScenario& doc)
{
    if(m_base)
    {
        m_base->baseConstraint()->stop();
    }
    m_base = std::make_unique<BaseScenarioElement>(doc, this);
}

void DocumentPlugin::clear()
{
    m_base.reset();
}

BaseScenarioElement *DocumentPlugin::baseScenario() const
{
    return m_base.get();
}
}
