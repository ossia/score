#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>

class QAbstractItemModel;

namespace PluginSettings
{
class BlacklistCommand;

class PluginSettingsModel : public iscore::SettingsDelegateModel
{
        Q_OBJECT
    public:
        PluginSettingsModel(const iscore::ApplicationContext& ctx);
        QAbstractItemModel* model()
        {
            return m_plugins;
        }

        void setFirstTimeSettings() override;

    signals:
        void blacklistCommand(BlacklistCommand*);


    private:
        QAbstractItemModel* m_plugins {};
};
}
