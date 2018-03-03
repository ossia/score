#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
class QCheckBox;
namespace Explorer::Settings
{
class View : public score::GlobalSettingsView
{
  Q_OBJECT
public:
  View();

  void setLocalTree(bool);
Q_SIGNALS:
  void localTreeChanged(bool);

  SETTINGS_UI_COMBOBOX_HPP(LogLevel)

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QCheckBox* m_cb{};
};
}

namespace Explorer::ProjectSettings
{
class View : public score::ProjectSettingsView
{
  Q_OBJECT
public:
  View();

  SETTINGS_UI_TOGGLE_HPP(ReconnectOnStart)
  SETTINGS_UI_TOGGLE_HPP(RefreshOnStart)

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

};
}
