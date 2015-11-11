#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include "Area/AreaModel.hpp"
#include "Area/AreaPresenter.hpp"
class QMainWindow;
class LayerView;
class SpaceLayerModel;
class SpaceLayerView;
class SpaceLayerPresenter : public LayerPresenter
{
    public:
        SpaceLayerPresenter(const LayerModel& model,
                            LayerView* view,
                            QObject* parent);
        ~SpaceLayerPresenter();

        void setWidth(int width);
        void setHeight(int height);

        void on_focusChanged();

        void putToFront();
        void putBehind();

        void on_zoomRatioChanged(ZoomRatio);
        void parentGeometryChanged();

        const LayerModel &layerModel() const;
        const Id<Process> &modelId() const;

        void update();

    private:
        void on_areaAdded(const AreaModel&);

        const SpaceLayerModel& m_model;
        SpaceLayerView* m_view;

        QMainWindow* m_spaceWindowView{};
        IdContainer<AreaPresenter, AreaModel> m_areas;
        FocusDispatcher m_focusDispatcher;


        // LayerPresenter interface
    public:
        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const override;
};
