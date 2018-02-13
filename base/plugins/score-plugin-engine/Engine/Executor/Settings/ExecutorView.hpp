#pragma once
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
class QSpinBox;
class QComboBox;
class QCheckBox;
namespace Engine
{
namespace Execution
{
namespace Settings
{
class View : public score::GlobalSettingsView
{
  Q_OBJECT
public:
  View();

  void setClock(ClockManagerFactory::ConcreteKey k);

  void populateClocks(
      const std::map<QString, ClockManagerFactory::ConcreteKey>&);

  SETTINGS_UI_COMBOBOX_HPP(Scheduling)
  SETTINGS_UI_COMBOBOX_HPP(Ordering)
  SETTINGS_UI_COMBOBOX_HPP(Merging)
  SETTINGS_UI_COMBOBOX_HPP(Commit)
  SETTINGS_UI_COMBOBOX_HPP(Tick)

  SETTINGS_UI_SPINBOX_HPP(Rate)

  SETTINGS_UI_TOGGLE_HPP(Logging)
  SETTINGS_UI_TOGGLE_HPP(Parallel)
  SETTINGS_UI_TOGGLE_HPP(ExecutionListening)
  SETTINGS_UI_TOGGLE_HPP(ScoreOrder)

Q_SIGNALS:
  void ClockChanged(ClockManagerFactory::ConcreteKey);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QComboBox* m_Clock{};

};
}
}
}
