#pragma once
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateView.hpp>
class QSpinBox;
class QComboBox;
namespace Engine
{
namespace Execution
{
namespace Settings
{

class View : public iscore::SettingsDelegateView
{
  Q_OBJECT
public:
  View();

  void setRate(int);
  void setClock(ClockManagerFactory::ConcreteFactoryKey k);

  void populateClocks(
      const std::map<QString, ClockManagerFactory::ConcreteFactoryKey>&);

signals:
  void rateChanged(int);
  void clockChanged(ClockManagerFactory::ConcreteFactoryKey);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QSpinBox* m_sb{};
  QComboBox* m_cb{};
};
}
}
}
