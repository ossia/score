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

// MOVEME
class PanelDelegateFactory final :
        public iscore::PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("ddbc5169-1ca3-4a64-a805-40b8fc0e1e02")

        std::unique_ptr<iscore::PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

