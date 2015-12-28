#pragma once
#include <Process/ZoomHelper.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>

class ProcessList;
class LayerModel;
class LayerPresenter;
class LayerView;
class ProcessPanelGraphicsProxy;
class QSize;
namespace iscore {
class PanelView;

}  // namespace iscore

class ProcessPanelPresenter final : public iscore::PanelPresenter
{
    public:
        ProcessPanelPresenter(
                const ProcessList& plist,
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;
        void on_modelChanged(
                iscore::PanelModel* oldm,
                iscore::PanelModel* newm) override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

    private:
        void on_focusedViewModelChanged(const LayerModel*);
        void on_sizeChanged(const QSize& size);
        void on_zoomChanged(ZoomRatio ratio);

        void cleanup();

        const ProcessList& m_processList;
        ProcessPanelGraphicsProxy* m_obj{};
        const LayerModel* m_layerModel{};
        LayerPresenter* m_processPresenter{};
        LayerView* m_layer{};

        ZoomRatio m_zoomRatio{};
};
