#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include "nano_observer.hpp"
#include "Area/AreaModel.hpp"
#include "Area/AreaPresenter.hpp"
class QMainWindow;
namespace Process {
class LayerModel;
class LayerView;
}

class SpaceLayerView;
class SpaceLayerPresenter :
        public Process::LayerPresenter,
        public Nano::Observer
{
    public:
        SpaceLayerPresenter(const Process::LayerModel& model,
                            Process::LayerView* view,
                            QObject* parent);
        ~SpaceLayerPresenter();

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

        void on_focusChanged() override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const Process::LayerModel &layerModel() const override;
        const Id<Process::ProcessModel> &modelId() const override;

        void update();

    private:
        void on_areaAdded(const AreaModel&);

        const Process::LayerModel& m_model;
        SpaceLayerView* m_view;

        const iscore::DocumentContext& m_ctx;
        QMainWindow* m_spaceWindowView{};
        IdContainer<AreaPresenter, AreaModel> m_areas;
        FocusDispatcher m_focusDispatcher;


        // LayerPresenter interface
    public:
        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const override;
};
