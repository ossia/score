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

#define SETTINGS_UI_COMBOBOX_HPP(Control)      \
  public: void set ## Control(QString);        \
  Q_SIGNALS: void Control ## Changed(QString); \
  private: QComboBox* m_ ## Control{};

#define SETTINGS_UI_TOGGLE_HPP(Control)        \
  public: void set ## Control(bool);           \
  Q_SIGNALS: void Control ## Changed(bool);    \
  private: QCheckBox* m_ ## Control{};

#define SETTINGS_UI_SPINBOX_HPP(Control)        \
  public: void set ## Control(int);            \
  Q_SIGNALS: void Control ## Changed(int);     \
  private: QSpinBox* m_ ## Control{};

class View : public score::SettingsDelegateView
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
