#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

class QCheckBox;
namespace Engine
{
namespace LocalTree
{
namespace Settings
{

class View : public score::SettingsDelegateView
{
  Q_OBJECT
public:
  View();

  void setLocalTree(bool);
Q_SIGNALS:
  void localTreeChanged(bool);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QCheckBox* m_cb{};
};
}
}
}
