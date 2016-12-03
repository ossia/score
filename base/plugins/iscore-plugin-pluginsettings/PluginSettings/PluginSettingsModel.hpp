#pragma once
#include <PluginSettings/PluginItemModel.hpp>
#include <QItemSelectionModel>
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>

class QAbstractItemModel;

namespace PluginSettings
{
class BlacklistCommand;

class PluginSettingsModel : public iscore::SettingsDelegateModel
{
public:
  PluginSettingsModel(QSettings& set, const iscore::ApplicationContext& ctx);

  LocalPluginItemModel localPlugins;
  RemotePluginItemModel remotePlugins;
  QItemSelectionModel remoteSelection;
};
}
