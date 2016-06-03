#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateView.hpp>

class QCheckBox;
namespace Ossia
{
namespace LocalTree
{
namespace Settings
{

class View :
        public iscore::SettingsDelegateView
{
        Q_OBJECT
    public:
        View();

        void setLocalTree(bool);
    signals:
        void localTreeChanged(bool);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QCheckBox* m_cb{};

};

}
}
}
