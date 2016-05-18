#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace iscore
{
class UndoListWidget;
class UndoPanelDelegate final :
        public iscore::PanelDelegate
{
    public:
        UndoPanelDelegate(
                const iscore::ApplicationContext& ctx);

    private:
        QWidget *widget() override;

        const PanelStatus& defaultPanelStatus() const override;

        void on_modelChanged(
                maybe_document_t oldm,
                maybe_document_t newm) override;

        iscore::UndoListWidget *m_list{};
        QWidget *m_widget{};
};

// MOVEME
class ISCORE_LIB_BASE_EXPORT UndoPanelDelegateFactory final :
        public PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("293c0f8b-fcb4-4554-8425-4bc03d803c75")

        std::unique_ptr<PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

