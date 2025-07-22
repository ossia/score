#pragma once
#include <Clap/EffectModel.hpp>

#include <score/widgets/PluginWindow.hpp>

#include <clap/all.h>

#include <memory>
#include <verdigris>

namespace Clap
{

class Window final : public score::PluginWindow
{
public:
  Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent);
  ~Window() override;
  
  void resize(int w, int h);

private:
  bool createClapWindow();
  void destroyClapWindow();
  
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  
  static void setup_rect(QWidget* container, int width, int height);
  void refreshTimer();
  
  bool queryExtensions();
  void initializeGui();
  
  const Model& m_model;
  PluginHandle* m_handle{};
  
  // CLAP GUI extension
  const clap_plugin_gui_t* m_gui_ext{};

  // Window state
  bool m_gui_created{false};
  bool m_gui_visible{false};
  bool m_is_floating{false};
  
  // Platform specific window handle
  clap_window_t m_clap_window{};
  
  // GUI API being used
  std::string m_gui_api;
};

}
