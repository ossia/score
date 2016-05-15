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
        ISCORE_CONCRETE_FACTORY_DECL("f418e1a0-fdff-45ec-99b2-b208706badc8")

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
