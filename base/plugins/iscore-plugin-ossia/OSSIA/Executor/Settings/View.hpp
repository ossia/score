#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

namespace RecreateOnPlay
{
namespace Settings
{

class View :
        public iscore::SettingsDelegateViewInterface
{
        Q_OBJECT
    public:
        View();

    signals:
        void rateChanged(int);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

};

}
}
