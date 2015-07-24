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
}

void SpaceLayerPresenter::setHeight(int height)
{
}

void SpaceLayerPresenter::putToFront()
{
}

void SpaceLayerPresenter::putBehind()
{
}

void SpaceLayerPresenter::on_zoomRatioChanged(ZoomRatio)
{
}

void SpaceLayerPresenter::parentGeometryChanged()
{
}

const LayerModel &SpaceLayerPresenter::viewModel() const
{
}

const id_type<Process> &SpaceLayerPresenter::modelId() const
{
}
