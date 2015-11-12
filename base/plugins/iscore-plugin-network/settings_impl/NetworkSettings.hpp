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
        iscore::SettingsDelegateViewInterface* makeView() override;
        iscore::SettingsDelegatePresenterInterface* makePresenter(iscore::SettingsPresenter*,
                iscore::SettingsDelegateModelInterface* m,
                iscore::SettingsDelegateViewInterface* v) override;
        iscore::SettingsDelegateModelInterface* makeModel() override;
};

