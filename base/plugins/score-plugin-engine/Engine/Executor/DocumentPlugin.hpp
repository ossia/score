#pragma once
#include "BaseScenarioComponent.hpp"

#include <Engine/Executor/ExecutorContext.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>
#include <score_plugin_engine_export.h>
#include <memory>

namespace score
{
class DocumentModel;
}
namespace Scenario
{
class BaseScenario;
class IntervalModel;
}
namespace Engine
{
namespace Execution
{
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final
    : public score::DocumentPlugin
{
public:
  DocumentPlugin(
      const score::DocumentContext& ctx, Id<score::DocumentPlugin>, QObject* parent);

  ~DocumentPlugin();
  void reload(Scenario::IntervalModel& doc);
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
