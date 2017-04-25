#pragma once
#include "BaseScenarioComponent.hpp"

#include <Engine/Executor/ExecutorContext.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore_plugin_engine_export.h>
#include <memory>

namespace iscore
{
class DocumentModel;
}
namespace Scenario
{
class BaseScenario;
class ConstraintModel;
}
namespace Engine
{
namespace Execution
{
class ISCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final
    : public iscore::DocumentPlugin
{
public:
  DocumentPlugin(
      const iscore::DocumentContext& ctx, Id<iscore::DocumentPlugin>, QObject* parent);

  ~DocumentPlugin();
  void reload(Scenario::ConstraintModel& doc);
  void clear();

  void on_documentClosing() override;
  const BaseScenarioElement& baseScenario() const;

  bool isPlaying() const;

  const Context& context() const
  {
    return m_ctx;
  }

  void runAllCommands() const;

private:
  mutable ExecutionCommandQueue m_editionQueue;
  Context m_ctx;
  BaseScenarioElement m_base;
};
}
}
