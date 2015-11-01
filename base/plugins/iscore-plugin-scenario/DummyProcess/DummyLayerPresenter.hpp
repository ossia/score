#pragma once
#include <Process/LayerPresenter.hpp>

class LayerView;
class DummyLayerPresenter final : public LayerPresenter
{
    public:
        explicit DummyLayerPresenter(
                const LayerModel& model,
                QObject* parent);

        void setWidth(int width) override;
        void setHeight(int height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;

        void parentGeometryChanged() override;

        const LayerModel& layerModel() const override;
        const Id<Process>& modelId() const override;

        void fillContextMenu(
                QMenu*,
                const QPoint& pos,
                const QPointF& scenepos) const override;

    private:
        const LayerModel& m_layer;
};
