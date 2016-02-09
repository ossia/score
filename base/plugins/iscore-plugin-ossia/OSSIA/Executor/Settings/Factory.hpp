#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

namespace RecreateOnPlay
{
namespace Settings
{

class Factory :
        public iscore::SettingsDelegateFactory
{
    private:
        iscore::SettingsDelegateViewInterface *makeView() override;
        iscore::SettingsDelegatePresenterInterface* makePresenter(
                iscore::SettingsDelegateModelInterface& m,
                iscore::SettingsDelegateViewInterface& v,
                QObject* parent) override;
        iscore::SettingsDelegateModelInterface *makeModel() override;
};

}
}
