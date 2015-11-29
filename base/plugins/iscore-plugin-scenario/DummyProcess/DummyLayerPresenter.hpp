#pragma once
#include <Process/LayerPresenter.hpp>
#include <qpoint.h>

#include "Process/ZoomHelper.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

class DummyLayerView;
class LayerModel;
class Process;
class QMenu;
class QObject;

class DummyLayerPresenter final : public LayerPresenter
{
    public:
        explicit DummyLayerPresenter(
                const LayerModel& model,
                DummyLayerView* view,
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
        DummyLayerView* m_view{};
};
