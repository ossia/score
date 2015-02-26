#pragma once
#include <set>
#include <memory>
#include <queue>
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <core/presenter/command/Command.hpp>
#include <QDebug>

namespace iscore
{
    class SettingsModel;
    class SettingsView;
    class SettingsPresenter : public QObject
    {
            Q_OBJECT
        public:
            SettingsPresenter (SettingsModel* model, SettingsView* view, QObject* parent);

            void addSettingsPresenter (SettingsDelegatePresenterInterface* presenter);

        private slots:
            void on_accept();
            void on_reject();

        private:
            SettingsModel* m_model;
            SettingsView* m_view;

            std::set<SettingsDelegatePresenterInterface*> m_pluginPresenters;
    };
}
