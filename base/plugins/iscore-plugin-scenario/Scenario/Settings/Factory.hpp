#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

namespace Scenario
{
namespace Settings
{

class Factory :
        public iscore::SettingsDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("27ab096d-b9df-4ca9-9442-1ebd697a8fab")

        iscore::SettingsDelegateViewInterface *makeView() override;
        iscore::SettingsDelegatePresenterInterface* makePresenter_impl(
                iscore::SettingsDelegateModelInterface& m,
                iscore::SettingsDelegateViewInterface& v,
                QObject* parent) override;
        iscore::SettingsDelegateModelInterface *makeModel() override;
};

}
}
