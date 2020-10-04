#pragma once
#include <PackageManager/PluginItemModel.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <QItemSelectionModel>

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
  QItemSelectionModel remoteSelection;
};
}
