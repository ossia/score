#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

#include <QIcon>
#include <QString>

#include <verdigris>

namespace PluginSettings
{
class BlacklistCommand;
class PluginSettingsModel;
class PluginSettingsView;
class PluginSettingsPresenter : public score::GlobalSettingsPresenter
{
  W_OBJECT(PluginSettingsPresenter)
public:
  PluginSettingsPresenter(
      score::SettingsDelegateModel& model,
      score::GlobalSettingsView& view,
      QObject* parent);

private:
  QString settingsName() override { return tr("Plugin"); }

  QIcon settingsIcon() override;
};
}
