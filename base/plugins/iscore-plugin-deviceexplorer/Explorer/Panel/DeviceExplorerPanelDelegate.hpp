#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>
namespace Explorer
{
class DeviceExplorerWidget;
class PanelDelegate final :
        public iscore::PanelDelegate
{
    public:
        PanelDelegate(
                const iscore::ApplicationContext& ctx);

    private:
        QWidget *widget() override;

        const iscore::PanelStatus& defaultPanelStatus() const override;

        void on_modelChanged(
                maybe_document_t oldm,
                maybe_document_t newm) override;

        DeviceExplorerWidget* m_widget{};
};
}
