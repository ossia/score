#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/Interface.hpp>
#include <verdigris>
#include <score_plugin_media_export.h>

class QTabWidget;


namespace Media::Settings
{
class View
    : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

private:
  QWidget* getWidget() override;

  QTabWidget* m_widg{};
};

class SCORE_PLUGIN_MEDIA_EXPORT PluginSettingsTab
    : public QObject
    , public score::InterfaceBase
{
  SCORE_INTERFACE(PluginSettingsTab, "a0ba4ef1-a448-45a5-b322-c1913c9b06a4")
public:
  ~PluginSettingsTab() override;

  virtual QString name() const noexcept = 0;
  virtual QWidget* make(const score::ApplicationContext& ctx) = 0;
};

class PluginSettingsFactoryList final
    : public score::InterfaceList<PluginSettingsTab>
{
};
}
