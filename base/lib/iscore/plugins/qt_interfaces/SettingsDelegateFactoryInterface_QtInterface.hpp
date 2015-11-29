#pragma once

namespace iscore
{
    class SettingsDelegateFactoryInterface;

    // A plug-in can also offer global settings for everything it provides.
    class SettingsDelegateFactoryInterface_QtInterface
    {
        public:
            virtual ~SettingsDelegateFactoryInterface_QtInterface();
            virtual SettingsDelegateFactoryInterface* settings_make() = 0;
    };
}


#define SettingsDelegateFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.SettingsDelegateFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::SettingsDelegateFactoryInterface_QtInterface, SettingsDelegateFactoryInterface_QtInterface_iid)
