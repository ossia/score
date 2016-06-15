#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>
class QListWidget;
namespace Ossia
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

        void on_modelChanged(
                iscore::MaybeDocument oldm,
                iscore::MaybeDocument newm) override;

        QListWidget *m_widget{};

        QMetaObject::Connection m_inbound, m_outbound;
};

class PanelDelegateFactory final :
        public iscore::PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("84a66cbe-aee3-496a-b7f4-0ea0d699deac")

        std::unique_ptr<iscore::PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};
}
