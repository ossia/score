#pragma once

namespace iscore
{
    class SettingsPresenter;
    class SettingsDelegatePresenterInterface;
    class SettingsDelegateModelInterface;
    class SettingsDelegateViewInterface;

    /**
     * @brief The SettingsDelegateFactoryInterface class
     *
     * Reimplement in order to provide custom settings for the plug-in.
     */
    class SettingsDelegateFactoryInterface
    {
        public:
            virtual ~SettingsDelegateFactoryInterface() = default;
            virtual SettingsDelegateViewInterface* makeView() = 0;
            virtual SettingsDelegatePresenterInterface* makePresenter (SettingsPresenter*,
                    SettingsDelegateModelInterface* m,
                    SettingsDelegateViewInterface* v) = 0;
            virtual SettingsDelegateModelInterface* makeModel() = 0; // Accédé par les commandes uniquement.
    };
}
