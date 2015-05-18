#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <QObject>


class NetworkSettings : public iscore::SettingsDelegateFactoryInterface
{
    public:
        NetworkSettings();
        virtual ~NetworkSettings() = default;

        // SettingsGroup interface
    public:
        virtual iscore::SettingsDelegateViewInterface* makeView() override;
        virtual iscore::SettingsDelegatePresenterInterface* makePresenter(iscore::SettingsPresenter*,
                iscore::SettingsDelegateModelInterface* m,
                iscore::SettingsDelegateViewInterface* v) override;
        virtual iscore::SettingsDelegateModelInterface* makeModel() override;
};

