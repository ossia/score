#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>

class QStandardItem;
class QStandardItemModel;

namespace iscore
{
}

namespace PluginSettings
{
class BlacklistCommand;

class PluginSettingsModel : public iscore::SettingsDelegateModel
{
        Q_OBJECT
    public:
        PluginSettingsModel(const iscore::ApplicationContext& ctx);
        QStandardItemModel* model()
        {
            return m_plugins;
        }

        void setFirstTimeSettings() override;

    signals:
        void blacklistCommand(BlacklistCommand*);
    public slots:
        void on_itemChanged(QStandardItem*);

    private:
        QStandardItemModel* m_plugins {};
};
}
