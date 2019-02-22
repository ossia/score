#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <Audio/AudioInterface.hpp>
#include <wobjectdefs.h>
class QCheckBox;
namespace Audio::Settings
{
class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void addDriver(QString txt, QVariant data, QWidget* widg);

  void setDriver(AudioFactory::ConcreteKey k);
  void DriverChanged(AudioFactory::ConcreteKey arg_1)
      W_SIGNAL(DriverChanged, arg_1);

  SETTINGS_UI_NUM_COMBOBOX_HPP(BufferSize)
  SETTINGS_UI_NUM_COMBOBOX_HPP(Rate)

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};
  QStackedWidget* m_sw{};
  QComboBox* m_Driver{};
};
}
