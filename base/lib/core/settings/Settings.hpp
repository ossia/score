#pragma once
#include <QObject>

#include <iscore_lib_base_export.h>
namespace iscore {
class SettingsDelegateFactoryInterface;
class SettingsModel;
class SettingsPresenter;
class SettingsView;
}  // namespace iscore

namespace iscore
{
    // Les settings ont leur propre "file de commande" pour ne pas interférer avec le reste.
    // (on ne "undo" généralement pas les settings).
    /**
     * @brief The Settings class
     *
     * They do not fit in the other MVP parts of the software due to the
     * command application difference.
     *
     * When "ok" is pressed the plug-ins are required to commit their changes to
     * their respective models.
     * Else they discard them.
     *
     * An exemple is given in the network plugin.
     *
     */
    class ISCORE_LIB_BASE_EXPORT Settings final : public QObject
    {
        public:
            Settings(QObject* parent);
            ~Settings();

            void setupSettingsPlugin(SettingsDelegateFactoryInterface* plugin);
            SettingsView*  view()
            {
                return m_settingsView;
            }
            SettingsModel* model()
            {
                return m_settingsModel;
            }

        private:
            SettingsModel* m_settingsModel;
            SettingsView* m_settingsView;
            SettingsPresenter* m_settingsPresenter;
    };
}
