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
                iscore::MaybeDocument oldm,
                iscore::MaybeDocument newm) override;

        void on_focusedViewModelChanged(
                const Process::LayerModel* theLM);


        void cleanup();

        QWidget* m_widget{};
        const Process::LayerModel* m_layerModel{};
        Process::LayerModelPanelProxy* m_proxy{};

        std::vector<QMetaObject::Connection> m_connections;
};

}

