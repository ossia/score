// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Focus/FocusDispatcher.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/detail/math.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Spline/Model.hpp>
#include <Spline/Presenter.hpp>
#include <Spline/View.hpp>
#include <QTimer>
#include <score/tools/Bind.hpp>
#include <wobjectimpl.h>
namespace Spline
{
Presenter::Presenter(
    const Spline::ProcessModel& layer,
    View* view,
    const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}, m_view{view}
{
  putToFront();
  con(layer, &ProcessModel::splineChanged, this, [&] { m_view->setSpline(layer.spline()); });

  m_view->setSpline(layer.spline());
  connect(m_view, &View::changed, this, [&] {
    context().context.dispatcher.submit<ChangeSpline>(layer, m_view->spline());
  });
  connect(m_view, &View::released, this, [&] {
    context().context.dispatcher.commit();
  });

  connect(m_view, &View::pressed, this, [&] { m_context.context.focusDispatcher.focus(this); });
  connect(m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);

  if (auto itv = Scenario::closestParentInterval(layer.parent()))
  {
    auto& dur = itv->duration;
    con(ctx.execTimer, &QTimer::timeout, this, [this, &dur] {
      {
        float p = ossia::clamp((float)dur.playPercentage(), 0.f, 1.f);
        ((View*)m_view)->setPlayPercentage(p);
      }
    });

    con(layer, &ProcessModel::resetExecution, this, [this] {
      ((View*)m_view)->setPlayPercentage(0.f);
    });
  }
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
  m_view->recenter();
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
  m_view->recenter();
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
  parentGeometryChanged();
}

void Presenter::parentGeometryChanged() { }

}
