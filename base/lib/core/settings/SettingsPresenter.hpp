#pragma once
#include <qobject.h>
#include <set>

namespace iscore {
class SettingsDelegatePresenterInterface;
}  // namespace iscore

namespace iscore
{
    class SettingsModel;
    class SettingsView;

    class SettingsPresenter final : public QObject
    {
            Q_OBJECT
        public:
            SettingsPresenter(SettingsModel* model, SettingsView* view, QObject* parent);

            void addSettingsPresenter(SettingsDelegatePresenterInterface* presenter);

        private slots:
            void on_accept();
            void on_reject();

        private:
            SettingsModel* m_model;
            SettingsView* m_view;

            std::set<SettingsDelegatePresenterInterface*> m_pluginPresenters;
    };
}
