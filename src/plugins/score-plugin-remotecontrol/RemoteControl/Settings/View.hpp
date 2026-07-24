#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <RemoteControl/Settings/Model.hpp>
class QCheckBox;
class QLineEdit;
class QVBoxLayout;

namespace score
{
class FormWidget;
}
namespace RemoteControl
{
namespace Settings
{

class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();
  void setEnabled(bool);
  void setWebUiPath(const QString&);
  void setServerAddress(const QString&);
  void setServerPort(unsigned short);
  void setServerEnabled(bool);

  void enabledChanged(bool b) W_SIGNAL(enabledChanged, b);
  void webUiPathChanged(QString s) W_SIGNAL(webUiPathChanged, s);
  void serverAddressChanged(QString s) W_SIGNAL(serverAddressChanged, s);
  void serverPortChanged(unsigned short s) W_SIGNAL(serverPortChanged, s);
  void serverEnabledChanged(bool b) W_SIGNAL(serverEnabledChanged, b);

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};

  QCheckBox* m_enabled{};

  score::FormWidget* m_web_ui{};
  QLineEdit* m_web_ui_path{};
  QLineEdit* m_server_address{};
  QSpinBox* m_server_port{};
  QCheckBox* m_server_enabled{};
};

}
}
