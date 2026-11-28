#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <score_plugin_gfx_export.h>

#include <verdigris>

namespace score
{
class FormWidget;
}
namespace Gfx::Settings
{
class SCORE_PLUGIN_GFX_EXPORT View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  SETTINGS_UI_COMBOBOX_HPP(GraphicsApi)
  SETTINGS_UI_COMBOBOX_HPP(HardwareDecode)
  SETTINGS_UI_NUM_COMBOBOX_HPP(DecodingThreads)
  SETTINGS_UI_DOUBLE_SPINBOX_HPP(Rate)
  SETTINGS_UI_NUM_COMBOBOX_HPP(Samples)
  SETTINGS_UI_TOGGLE_HPP(VSync)
  SETTINGS_UI_NUM_COMBOBOX_HPP(Buffers)

public:
  void setGpu(QString);
  void GpuChanged(QString arg) W_SIGNAL(GpuChanged, arg);

private:
  QWidget* getWidget() override;
  void populateGpus();
  QComboBox* m_Gpu{};
  score::FormWidget* m_widg{};
  bool m_gpusPopulated{};
};

}
