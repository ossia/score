#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <verdigris>

namespace score {class FormWidget;}
namespace Gfx::Settings
{
class View
    : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  SETTINGS_UI_COMBOBOX_HPP(GraphicsApi)
  SETTINGS_UI_DOUBLE_SPINBOX_HPP(Rate)
  SETTINGS_UI_TOGGLE_HPP(VSync)

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};
};

}
