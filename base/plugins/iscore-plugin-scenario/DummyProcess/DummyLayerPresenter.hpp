#pragma once
#include <Process/LayerPresenter.hpp>
#include <QPoint>

#include <Process/ZoomHelper.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_dummyprocess_export.h>


class DummyLayerView;
class LayerModel;
class Process;
class QMenu;
class QObject;

class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerPresenter final : public LayerPresenter
{
    public:
        explicit DummyLayerPresenter(
                const LayerModel& model,
                DummyLayerView* view,
                QObject* parent);

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

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
