#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
class QCheckBox;
namespace Media::Settings
{
class View : public score::GlobalSettingsView
{
  Q_OBJECT
public:
  View();

public:
  void setVstPaths(QStringList);
Q_SIGNALS:
  void VstPathsChanged(QStringList);

private:
  QListWidget* m_VstPaths{};

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};
  QStringList m_curitems;
};
}
