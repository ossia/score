#pragma once
#include <ProcessInterface/LayerPresenter.hpp>

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

        const LayerModel &viewModel() const;
        const id_type<Process> &modelId() const;

    private:
        const SpaceLayerModel& m_model;
        SpaceLayerView* m_view;
};
