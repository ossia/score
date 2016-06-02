#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenterInterface.hpp>

namespace RecreateOnPlay
{
namespace Settings
{
class Model;
class View;
class Presenter :
        public iscore::SettingsDelegatePresenter
{
    public:
        using model_type = Model;
        using view_type = View;
        Presenter(Model&, View&, QObject* parent);

    private:
        QString settingsName() override;
        QIcon settingsIcon() override;
};

}
}
