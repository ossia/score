#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include "ProcessPanelGraphicsProxy.hpp"

#include <Process/ZoomHelper.hpp>
class BaseElementModel;
class LayerView;
class LayerPresenter;
class LayerModel;
class ProcessPanelPresenter : public iscore::PanelPresenter
{
        Q_OBJECT
    public:
        ProcessPanelPresenter(iscore::Presenter* parent_presenter,
                              iscore::PanelView* view);

        int panelId() const override;
        void on_modelChanged() override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

    private slots:
        void on_focusedViewModelChanged(const LayerModel*);
        void on_sizeChanged(const QSize& size);
        void on_zoomChanged(ZoomRatio ratio);

    private:
        void cleanup();

        ProcessPanelGraphicsProxy* m_obj{};
        const LayerModel* m_layerModel{};
        LayerPresenter* m_processPresenter{};
        LayerView* m_layer{};

        ZoomRatio m_zoomRatio{};
};
