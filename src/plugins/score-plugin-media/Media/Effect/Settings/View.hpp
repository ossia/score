#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <wobjectdefs.h>
class QCheckBox;
namespace Media::Settings
{
class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

public:
  void setVstPaths(QStringList);

public:
  void VstPathsChanged(QStringList arg_1) W_SIGNAL(VstPathsChanged, arg_1);

private:
  QListWidget* m_VstPaths{};

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};
  QStringList m_curitems;
};
}
