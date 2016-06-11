#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <QItemSelectionModel>
#include <PluginSettings/PluginItemModel.hpp>

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
