#pragma once
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <score_plugin_gfx_export.h>

namespace Gfx
{
class SCORE_PLUGIN_GFX_EXPORT DocumentPlugin final
    : public score::DocumentPlugin
{
public:
  DocumentPlugin(
      const score::DocumentContext& doc,
      QObject* parent);
  ~DocumentPlugin() override;

  GfxContext context;
  GfxExecutionAction exec{context};
};

class ApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);

protected:
  void on_createdDocument(score::Document& doc) override;
};
}
