// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/Presenter.hpp>
#include <Gfx/Settings/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::Settings::View)
namespace Gfx::Settings
{
View::View()
{
  m_widg = new score::FormWidget{tr("Graphics")};

  auto lay = m_widg->layout();
  SETTINGS_UI_COMBOBOX_SETUP("Graphics API", GraphicsApi, GraphicsApis{});
  SETTINGS_UI_COMBOBOX_SETUP(
      "Hardware Video Decoding", HardwareDecode, HardwareVideoDecoder{});

  static constexpr int t_values[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  SETTINGS_UI_NUM_COMBOBOX_SETUP("Decoding threads", DecodingThreads, t_values);

  static constexpr int aa_values[]{1, 2, 4, 8, 16};
  SETTINGS_UI_NUM_COMBOBOX_SETUP("Multisampling AA", Samples, aa_values);
  SETTINGS_UI_DOUBLE_SPINBOX_SETUP("Rate (if no VSync)", Rate);
  m_Rate->setRange(1., 1000.);
  SETTINGS_UI_TOGGLE_SETUP("VSync", VSync);

  static constexpr int buffers_values[]{1, 2, 3};
  SETTINGS_UI_NUM_COMBOBOX_SETUP("Buffer count", Buffers, buffers_values);
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_COMBOBOX_IMPL(GraphicsApi)
SETTINGS_UI_COMBOBOX_IMPL(HardwareDecode)
SETTINGS_UI_NUM_COMBOBOX_IMPL(DecodingThreads)
SETTINGS_UI_NUM_COMBOBOX_IMPL(Samples)
SETTINGS_UI_DOUBLE_SPINBOX_IMPL(Rate)
SETTINGS_UI_TOGGLE_IMPL(VSync)
SETTINGS_UI_NUM_COMBOBOX_IMPL(Buffers)
}
