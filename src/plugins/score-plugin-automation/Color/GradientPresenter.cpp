// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Focus/FocusDispatcher.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/detail/math.hpp>

#include <Color/GradientModel.hpp>
#include <Color/GradientPresenter.hpp>
#include <Color/GradientView.hpp>
#include <wobjectimpl.h>
namespace Gradient
{
Presenter::Presenter(
    const Gradient::ProcessModel& layer,
    View* view,
    const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}, m_layer{layer}, m_view{view}
{
  putToFront();
  connect(&m_layer, &ProcessModel::gradientChanged, this, [&] {
    m_view->setGradient(m_layer.gradient());
  });

  m_view->setGradient(m_layer.gradient());
  connect(m_view, &View::doubleClicked, this, [&](QPointF pos) {
    auto np = pos.x() / m_view->dataWidth();
    auto new_grad = m_layer.gradient();
    auto prev = new_grad.lower_bound(np);
    if (prev == new_grad.begin())
      return;
    if (prev == new_grad.end())
      prev = new_grad.begin();
    else
      prev--;

    new_grad.insert(std::make_pair(np, prev->second));
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::movePoint, this, [&](double orig, double cur) {
    auto new_grad = m_layer.gradient();
    auto it = new_grad.find(orig);
    if (it == new_grad.end())
      return;

    auto col = it->second;
    new_grad.erase(it);
    new_grad.insert(std::make_pair(cur, col));
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::removePoint, this, [&](double orig) {
    auto new_grad = m_layer.gradient();
    auto it = new_grad.find(orig);
    if (it == new_grad.end())
      return;

    new_grad.erase(it);
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::setColor, this, [&](double pos, QColor col) {
    auto new_grad = m_layer.gradient();
    auto it = new_grad.has(pos);
    if (!it)
      return;

    *it = col;
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::pressed, this, [&] {
    m_context.context.focusDispatcher.focus(this);
  });
  connect(
      m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
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
  m_view->setDataWidth(m_layer.duration().toPixels(m_zoomRatio));
}

const Gradient::ProcessModel& Presenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_layer.id();
}
}
