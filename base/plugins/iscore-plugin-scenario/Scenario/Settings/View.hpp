#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

class QComboBox;
namespace Scenario
{
namespace Settings
{

class View :
        public iscore::SettingsDelegateViewInterface
{
        Q_OBJECT
    public:
        View();

        void setSkin(const QString&);

    signals:
        void skinChanged(const QString&);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QComboBox* m_skin{};

};

}
}
