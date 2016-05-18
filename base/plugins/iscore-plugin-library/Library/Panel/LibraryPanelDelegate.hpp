#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>
class QTabWidget;
namespace Library
{
class PanelDelegate final :
        public iscore::PanelDelegate
{
    public:
        PanelDelegate(
                const iscore::ApplicationContext& ctx);

    private:
        QWidget *widget() override;

        const iscore::PanelStatus& defaultPanelStatus() const override;

        QTabWidget *m_widget{};
};
}
