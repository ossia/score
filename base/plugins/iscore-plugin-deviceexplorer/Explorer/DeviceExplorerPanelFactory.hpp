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

// MOVEME
class PanelDelegateFactory final :
        public iscore::PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("de755995-398d-4b16-9030-574533b17a9f")

        std::unique_ptr<iscore::PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

