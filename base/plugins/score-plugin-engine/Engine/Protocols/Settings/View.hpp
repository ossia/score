#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <wobjectdefs.h>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
class QCheckBox;
namespace Audio::Settings
{
class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();
  SETTINGS_UI_COMBOBOX_HPP(Driver)
  SETTINGS_UI_COMBOBOX_HPP(CardIn)
  SETTINGS_UI_COMBOBOX_HPP(CardOut)
  SETTINGS_UI_NUM_COMBOBOX_HPP(BufferSize)
  SETTINGS_UI_NUM_COMBOBOX_HPP(Rate)

  SETTINGS_UI_SPINBOX_HPP(DefaultIn)
  SETTINGS_UI_SPINBOX_HPP(DefaultOut)

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};
};
}
