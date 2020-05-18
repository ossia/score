#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <RemoteControl/Settings/Model.hpp>
class QCheckBox;

namespace score{ class FormWidget;}
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
  score::FormWidget* m_widg{};

  QCheckBox* m_enabled{};
};

}
}
