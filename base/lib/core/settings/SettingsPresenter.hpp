#pragma once
#include <QObject>
#include <set>

namespace iscore {
class SettingsDelegatePresenter;
}  // namespace iscore

namespace iscore
{
    class Settings;
    class SettingsView;

    class SettingsPresenter final : public QObject
    {
            Q_OBJECT
        public:
            SettingsPresenter(SettingsView* view, QObject* parent);

            void addSettingsPresenter(SettingsDelegatePresenter* presenter);

        private slots:
            void on_accept();
            void on_reject();

        private:
            SettingsView* m_view;

            std::set<SettingsDelegatePresenter*> m_pluginPresenters;
    };
}
