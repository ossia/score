#pragma once
#include <QIcon>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>


#include <QString>

namespace PluginSettings
{
class BlacklistCommand;
class PluginSettingsModel;
class PluginSettingsView;
class PluginSettingsPresenter : public score::GlobalSettingsPresenter
{
  Q_OBJECT
public:
  PluginSettingsPresenter(
      score::SettingsDelegateModel& model,
      score::GlobalSettingsView& view,
      QObject* parent);

private:
  QString settingsName() override
  {
    return tr("Plugin");
  }

  QIcon settingsIcon() override;
};
}
