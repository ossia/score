#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <verdigris>
class QStackedWidget;
class QCheckBox;

namespace score
{
class FormWidget;
}

namespace Protocols::Settings
{
class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  SETTINGS_UI_COMBOBOX_HPP(MidiAPI)

private:
  QWidget* getWidget() override;

  score::FormWidget* m_widg{};
};
}
