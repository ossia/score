#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include "BaseScenarioComponent.hpp"
#include "DocumentPlugin.hpp"
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/StateProcessComponent.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
class QObject;
namespace iscore
{
class Document;
} // namespace iscore

namespace Engine
{
namespace Execution
{
DocumentPlugin::DocumentPlugin(
    iscore::Document& doc, Id<iscore::DocumentPlugin> id, QObject* parent)
    : iscore::DocumentPlugin{doc.context(), std::move(id),
                             "OSSIADocumentPlugin", parent}
    , m_ctx{
          doc.context(), *this,
          doc.context().plugin<Explorer::DeviceDocumentPlugin>(),
          doc.context().app.interfaces<ProcessComponentFactoryList>(),
          doc.context()
              .app.interfaces<StateProcessComponentFactoryList>(),
          m_editionQueue
      }
{
  con(doc, &iscore::Document::aboutToClose, this, [&] {
    if (m_base)
    {
      m_base->baseConstraint().stop();
    }
    m_base.reset();
  });
}

DocumentPlugin::~DocumentPlugin()
{
  if (m_base)
  {
    m_base->baseConstraint().stop();
    clear();
  }
}

void DocumentPlugin::reload(Scenario::ConstraintModel& cst)
{
  if (m_base)
  {
    m_base->baseConstraint().stop();
  }
  clear();

  auto parent = dynamic_cast<Scenario::ScenarioInterface*>(cst.parent());
  ISCORE_ASSERT(parent);
  m_base = std::make_shared<BaseScenarioElement>(
      BaseScenarioRefContainer{cst, *parent}, m_ctx, this);

  runAllCommands();
}

void DocumentPlugin::clear()
{
  if(m_base)
    m_base->cleanup();
  m_base.reset();
}

BaseScenarioElement* DocumentPlugin::baseScenario() const
{
  return m_base.get();
}

bool DocumentPlugin::isPlaying() const
{
  return m_base.get();
}

void DocumentPlugin::runAllCommands()
{
  bool ok = false;
  ExecutionCommand com;
  do {
    ok = m_editionQueue.try_dequeue(com);
    if(ok && com)
      com();
  } while(ok);
}
}
}
