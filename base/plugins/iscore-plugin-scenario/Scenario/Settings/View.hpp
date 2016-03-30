#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

class QComboBox;
class QSpinBox;
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
        void setZoom(const int); // zoom percentage

    signals:
        void skinChanged(const QString&);
        void zoomChanged(int);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QComboBox* m_skin{};
        QSpinBox* m_zoomSpinBox{};
};

}
}
