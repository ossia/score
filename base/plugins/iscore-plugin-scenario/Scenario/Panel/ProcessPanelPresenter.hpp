#pragma once
#include <Process/ZoomHelper.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
namespace Process
{
class ProcessList;
class LayerModel;
class LayerPresenter;
class LayerView;
}

class ProcessPanelGraphicsProxy;
class QSize;

namespace iscore {
class PanelView;
}  // namespace iscore

class ProcessPanelPresenter final : public iscore::PanelPresenter
{
    public:
        ProcessPanelPresenter(
                const Process::ProcessList& plist,
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;
        void on_modelChanged(
                iscore::PanelModel* oldm,
                iscore::PanelModel* newm) override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

    private:
        void on_focusedViewModelChanged(const Process::LayerModel*);
        void on_sizeChanged(const QSize& size);
        void on_zoomChanged(ZoomRatio ratio);

        void cleanup();

        const Process::ProcessList& m_processList;
        ProcessPanelGraphicsProxy* m_obj{};
        const Process::LayerModel* m_layerModel{};
        Process::LayerPresenter* m_processPresenter{};
        Process::LayerView* m_layer{};

        ZoomRatio m_zoomRatio{};
};
