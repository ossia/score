#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace Process
{
class LayerModel;
class LayerModelPanelProxy;
}

namespace Scenario
{
class PanelDelegate final :
        public QObject,
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

        void on_focusedViewModelChanged(
                const Process::LayerModel* theLM);


        void cleanup();

        QWidget* m_widget{};
        const Process::LayerModel* m_layerModel{};
        Process::LayerModelPanelProxy* m_proxy{};

        std::vector<QMetaObject::Connection> m_connections;
};

// MOVEME
class PanelDelegateFactory final :
        public iscore::PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("c255f3db-3758-4d99-961d-76c1ffffc646")

        std::unique_ptr<iscore::PanelDelegate> make(
                const iscore::ApplicationContext& ctx) override;
};

}

