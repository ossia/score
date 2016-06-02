#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

class QSpinBox;
namespace RecreateOnPlay
{
namespace Settings
{

class View :
        public iscore::SettingsDelegateView
{
        Q_OBJECT
    public:
        View();

        void setRate(int);
    signals:
        void rateChanged(int);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QSpinBox* m_sb{};

};

}
}
