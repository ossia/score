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

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};
};

}
