// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Automation/Spline/SplineAutomPresenter.hpp>
#include <Automation/Spline/SplineAutomModel.hpp>
#include <Automation/Spline/SplineAutomView.hpp>

#include <Process/Focus/FocusDispatcher.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <ossia/detail/math.hpp>
namespace Spline
{
Presenter::Presenter(
    const Spline::ProcessModel& layer,
    View* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : LayerPresenter{ctx, parent}
    , m_layer{layer}
    , m_view{view}
{
  putToFront();
  connect(&m_layer, &ProcessModel::splineChanged, this, [&] {
    m_view->setSpline(m_layer.spline());
  });

  m_view->setSpline(m_layer.spline());
  connect(m_view, &View::changed, this, [&] {
    CommandDispatcher<>{context().context.commandStack}
          .submitCommand<ChangeSpline>(layer, m_view->spline());
  });

  connect(m_view, &View::pressed, this, [&] {
    m_context.context.focusDispatcher.focus(this);
  });
  connect(m_view, &View::askContextMenu,
          this, &Presenter::contextMenuRequested);
}

void Presenter::setWidth(qreal val)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setEnabled(true);
}

void Presenter::putBehind()
{
  m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio r)
{
  m_zoomRatio = r;
  parentGeometryChanged();
}

void Presenter::parentGeometryChanged()
{
}

const Spline::ProcessModel& Presenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_layer.id();
}
}
