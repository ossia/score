#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace InspectorPanel
{
class SelectionStackWidget;
class InspectorPanelWidget;

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
        void setNewSelection(const Selection& s) override;

        QWidget* m_widget{};
        SelectionStackWidget* m_stack{};
        InspectorPanelWidget* m_inspectorPanel {};
};

// MOVEME
class PanelDelegateFactory final :
        public iscore::PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("3c489368-c946-4f9f-8d6c-d051b724726c")

        std::unique_ptr<iscore::PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

