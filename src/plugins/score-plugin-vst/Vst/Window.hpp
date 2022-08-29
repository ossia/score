#pragma once
#include <Vst/EffectModel.hpp>

#include <score/widgets/PluginWindow.hpp>

#include <memory>
#include <verdigris>

namespace vst
{

class Window final : public score::PluginWindow
{
public:
  static ERect getRect(AEffect& e);

  Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent);
  ~Window() override;
  void resize(int w, int h);

  void initNativeWindow(const Model& e, const score::DocumentContext& ctx);

private:
  static void setup_rect(QWidget* container, int width, int height);
  void refreshTimer();

  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  std::weak_ptr<AEffectWrapper> effect;
  const Model& m_model;
};

}
