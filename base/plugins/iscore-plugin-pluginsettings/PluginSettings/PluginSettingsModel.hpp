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
        Q_OBJECT
    public:
        PluginSettingsModel(const iscore::ApplicationContext& ctx);

        LocalPluginItemModel localPlugins;
        RemotePluginItemModel remotePlugins;
        QItemSelectionModel remoteSelection;

        void setFirstTimeSettings() override;

    private:
};
}
