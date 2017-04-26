#include <Process/Process.hpp>

#include "DummyLayerPresenter.hpp"
#include "DummyLayerView.hpp"
#include <Process/LayerPresenter.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
class QMenu;
class QObject;

namespace Dummy
{
DummyLayerPresenter::DummyLayerPresenter(
    const Process::ProcessModel& model,
    DummyLayerView* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
{
  putToFront();
  connect(view, &DummyLayerView::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });
  con(model.metadata(), &iscore::ModelMetadata::NameChanged,
      this, [&](QString s) { putToFront(); });
}

void DummyLayerPresenter::setWidth(qreal val)
{
  m_view->setWidth(val);
}

void DummyLayerPresenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void DummyLayerPresenter::putToFront()
{
  m_view->setText(m_layer.prettyName());
}

void DummyLayerPresenter::putBehind()
{
  m_view->setText({});
}

void DummyLayerPresenter::on_zoomRatioChanged(ZoomRatio)
{
}

void DummyLayerPresenter::parentGeometryChanged()
{
}

const Process::ProcessModel& DummyLayerPresenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& DummyLayerPresenter::modelId() const
{
  return m_layer.id();
}
}
