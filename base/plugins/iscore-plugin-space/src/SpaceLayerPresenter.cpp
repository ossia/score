#include "SpaceLayerPresenter.hpp"
#include "SpaceLayerModel.hpp"
#include "SpaceLayerView.hpp"
SpaceLayerPresenter::SpaceLayerPresenter(const LayerModel& model, LayerView* view, QObject* parent):
    LayerPresenter{"LayerPresenter", parent},
    m_model{static_cast<const SpaceLayerModel&>(model)},
    m_view{static_cast<SpaceLayerView*>(view)}
{

}

SpaceLayerPresenter::~SpaceLayerPresenter()
{

}

void SpaceLayerPresenter::setWidth(int width)
{
    ISCORE_TODO;
}

void SpaceLayerPresenter::setHeight(int height)
{
    ISCORE_TODO;
}

void SpaceLayerPresenter::putToFront()
{
    ISCORE_TODO;
}

void SpaceLayerPresenter::putBehind()
{
    ISCORE_TODO;
}

void SpaceLayerPresenter::on_zoomRatioChanged(ZoomRatio)
{
    ISCORE_TODO;
}

void SpaceLayerPresenter::parentGeometryChanged()
{
    ISCORE_TODO;
}

const LayerModel &SpaceLayerPresenter::layerModel() const
{
    ISCORE_TODO;
    return m_model;
}

const id_type<Process> &SpaceLayerPresenter::modelId() const
{
    ISCORE_TODO;
}
