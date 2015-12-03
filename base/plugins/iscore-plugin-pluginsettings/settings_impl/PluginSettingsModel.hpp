#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>

class QStandardItem;
class QStandardItemModel;

namespace iscore
{
}
class BlacklistCommand;

class PluginSettingsModel : public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
    public:
        PluginSettingsModel();
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
