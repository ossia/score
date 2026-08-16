#pragma once
#include <LV2/EffectModel.hpp>

#include <score/widgets/PluginWindow.hpp>

#include <verdigris>

namespace LV2
{
//! True if this UI binary links a Qt major version other than the one this
//! process runs: such a UI cannot be loaded in-process (identical mangled
//! symbol names across Qt majors get cross-resolved and corrupt memory).
SCORE_PLUGIN_LV2_EXPORT
bool uiLinksIncompatibleQt(const QString& binary_path);

class Window final : public score::PluginWindow
{
  W_OBJECT(Window)
public:
  Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent);

  ~Window() override;
  // TODO void resize(int w, int h);
  static bool is_resizable(LilvWorld* world, const LilvUI& ui);

private:
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  const Model& m_model;
  QWidget* m_widget{};
};

using LayerFactory = Process::EffectLayerFactory_T<Model, Window>;
}
