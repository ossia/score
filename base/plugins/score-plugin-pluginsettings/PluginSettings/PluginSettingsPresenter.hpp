#pragma once
#include <QIcon>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

#include <QString>

namespace score
{
class Command;
class SettingsDelegateModel;
class SettingsDelegateView;
class SettingsPresenter;
} // namespace score

namespace PluginSettings
{
class BlacklistCommand;
class PluginSettingsModel;
class PluginSettingsView;
class PluginSettingsPresenter : public score::SettingsDelegatePresenter
{
  Q_OBJECT
public:
  PluginSettingsPresenter(
      score::SettingsDelegateModel& model,
      score::SettingsDelegateView& view,
      QObject* parent);

private:
  QString settingsName() override
  {
    return tr("Plugin");
  }

  QIcon settingsIcon() override;
};
}
