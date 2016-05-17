#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

namespace Ossia
{
namespace LocalTree
{
namespace Settings
{

class Factory :
        public iscore::SettingsDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("3cf335f6-8f5d-401b-98a3-eedfd5e7d292")

        iscore::SettingsDelegateViewInterface *makeView() override;
        iscore::SettingsDelegatePresenterInterface* makePresenter_impl(
                iscore::SettingsDelegateModelInterface& m,
                iscore::SettingsDelegateViewInterface& v,
                QObject* parent) override;
        iscore::SettingsDelegateModelInterface *makeModel() override;
};

}
}
}
