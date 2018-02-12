#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

class QCheckBox;
namespace Explorer::Settings
{
class View : public score::SettingsDelegateView
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
