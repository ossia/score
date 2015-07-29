#pragma once
#include <ProcessInterface/LayerPresenter.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include "Area/AreaModel.hpp"
#include "Area/AreaPresenter.hpp"
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

        void putToFront();
        void putBehind();

        void on_zoomRatioChanged(ZoomRatio);
        void parentGeometryChanged();

        const LayerModel &layerModel() const;
        const id_type<Process> &modelId() const;

        void update();
    private:
        void on_areaAdded(const AreaModel&);

        const SpaceLayerModel& m_model;
        SpaceLayerView* m_view;


        IdContainer<AreaPresenter, AreaModel> m_areas;
};
