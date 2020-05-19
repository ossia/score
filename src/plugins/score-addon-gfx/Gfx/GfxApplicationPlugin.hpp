#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <score_addon_gfx_export.h>

namespace Gfx
{
class SCORE_ADDON_GFX_EXPORT DocumentPlugin final : public score::DocumentPlugin
{
public:
  DocumentPlugin(const score::DocumentContext& doc, Id<score::DocumentPlugin> id, QObject* parent);
  ~DocumentPlugin() override;

  gfx_window_context context;
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
