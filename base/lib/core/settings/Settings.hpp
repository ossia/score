#pragma once
#include <QObject>
#include <memory>
#include <iscore_lib_base_export.h>
namespace iscore {
class SettingsDelegateFactory;
class SettingsDelegateModel;
class SettingsModel;
class SettingsPresenter;
class SettingsView;

class ApplicationContext;
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
    class ISCORE_LIB_BASE_EXPORT Settings final
    {
        public:
            Settings();
            ~Settings();

            void setupSettingsPlugin(
                    const iscore::ApplicationContext& ctx,
                    SettingsDelegateFactory& plugin);
            SettingsView& view() const
            {
                return *m_settingsView;
            }

            auto& settings() const
            { return m_settings; }
        private:

            SettingsView* m_settingsView{};
            SettingsPresenter* m_settingsPresenter{};

            std::vector<std::unique_ptr<SettingsDelegateModel>> m_settings;
    } ;
}
