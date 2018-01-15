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

class View : public score::SettingsDelegateView
{
  Q_OBJECT
public:
  View();

  void setRate(int);
  void setExecutionListening(bool);
  void setClock(ClockManagerFactory::ConcreteKey k);

  void populateClocks(
      const std::map<QString, ClockManagerFactory::ConcreteKey>&);

Q_SIGNALS:
  void rateChanged(int);
  void executionListeningChanged(bool);
  void clockChanged(ClockManagerFactory::ConcreteKey);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QSpinBox* m_sb{};
  QComboBox* m_cb{};
  QCheckBox* m_listening{};
};
}
}
}
