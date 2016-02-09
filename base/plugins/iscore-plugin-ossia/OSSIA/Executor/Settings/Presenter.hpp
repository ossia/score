#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenterInterface.hpp>

namespace RecreateOnPlay
{
namespace Settings
{
class Model;
class View;
class Presenter :
        public iscore::SettingsDelegatePresenterInterface
{
    public:
        using model_type = Model;
        using view_type = View;
        Presenter(Model&, View&, QObject* parent);

    private:
        void on_rateChanged(int);

        void on_accept() override;
        void on_reject() override;
        QString settingsName() override;
        QIcon settingsIcon() override;
};

}
}
