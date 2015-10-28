#pragma once
#include <ProcessInterface/LayerPresenter.hpp>

class DummyLayerPresenter : public LayerPresenter
{
    public:
        void setWidth(int width);
        void setHeight(int height);
        void putToFront();
        void putBehind();
        void on_zoomRatioChanged(ZoomRatio);
        void parentGeometryChanged();
        const LayerModel&layerModel() const;
        const Id<Process>&modelId() const;
        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const;
};
