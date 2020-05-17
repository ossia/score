#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <Audio/AudioInterface.hpp>

#include <verdigris>
class QStackedWidget;
class QCheckBox;
namespace Audio::Settings
{
class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void addDriver(QString txt, QVariant data, Audio::AudioFactory* widg);

  void setDriver(AudioFactory::ConcreteKey k);
  void setDriverWidget(QWidget* w);
  void DriverChanged(AudioFactory::ConcreteKey arg_1) W_SIGNAL(DriverChanged, arg_1);

  void setBufferSize(int);
  void BufferSizeChanged(int arg) W_SIGNAL(BufferSizeChanged, arg)
  void setRate(int);
  void RateChanged(int arg) W_SIGNAL(RateChanged, arg)

  SETTINGS_UI_TOGGLE_HPP(AutoStereo)

private:
  QWidget* getWidget() override;

  QWidget* m_widg{};
  QWidget* m_sw{};
  QComboBox* m_Driver{};
  QWidget* m_curDriver{};
};
}
