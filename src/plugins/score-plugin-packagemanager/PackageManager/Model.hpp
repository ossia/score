#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <PackageManager/PluginItemModel.hpp>

class QAbstractItemModel;

namespace PM
{
class BlacklistCommand;

class PluginSettingsModel : public score::SettingsDelegateModel
{
public:
  PluginSettingsModel(QSettings& set, const score::ApplicationContext& ctx);
  ~PluginSettingsModel();

  LocalPackagesModel localPlugins;
  RemotePackagesModel remotePlugins;
};
}
