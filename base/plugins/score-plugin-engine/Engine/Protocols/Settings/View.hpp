#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
class QCheckBox;
namespace Audio::Settings
{
class View : public score::GlobalSettingsView
{
  Q_OBJECT
public:
  View();
  SETTINGS_UI_COMBOBOX_HPP(CardIn)
  SETTINGS_UI_COMBOBOX_HPP(CardOut)
  SETTINGS_UI_NUM_COMBOBOX_HPP(BufferSize)
  SETTINGS_UI_NUM_COMBOBOX_HPP(Rate)

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

};
}
