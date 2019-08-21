#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <RemoteControl/Settings/Model.hpp>
class QCheckBox;
class QDoubleSpinBox;
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

  void enabledChanged(bool b) W_SIGNAL(enabledChanged, b);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QCheckBox* m_enabled{};
};

}
}
