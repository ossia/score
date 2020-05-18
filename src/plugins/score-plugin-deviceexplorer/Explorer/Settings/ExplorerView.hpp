#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <verdigris>
class QCheckBox;
namespace score{ class FormWidget;}
namespace Explorer::Settings
{
class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void setLocalTree(bool);

public:
  void localTreeChanged(int arg_1) W_SIGNAL(localTreeChanged, arg_1);

  SETTINGS_UI_COMBOBOX_HPP(LogLevel)

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};

  QCheckBox* m_cb{};
};
}

namespace Explorer::ProjectSettings
{
class View : public score::ProjectSettingsView
{
  W_OBJECT(View)
public:
  View();

  SETTINGS_UI_DOUBLE_SPINBOX_HPP(MidiImportRatio)
  SETTINGS_UI_TOGGLE_HPP(ReconnectOnStart)
  SETTINGS_UI_TOGGLE_HPP(RefreshOnStart)

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};
};
}
