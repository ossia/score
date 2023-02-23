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

ProcessModel::gradient_colors addPointToGradient(
    const ProcessModel::gradient_colors& current, double np, QColor* new_color = nullptr)
{

  auto new_grad = current;
  auto prev = new_grad.lower_bound(np);
  auto prev_color = prev->second;
  auto color_or = [=](QColor p) noexcept { return new_color ? *new_color : p; };
  if(prev == new_grad.begin())
  {
    new_grad.insert(std::make_pair(np, color_or(prev_color)));
  }
  if(prev == new_grad.end())
  {
    if(!new_grad.empty())
    {
      prev = new_grad.begin();
      new_grad.insert(std::make_pair(np, color_or(prev_color)));
    }
    else
    {
      new_grad.insert(std::make_pair(np, color_or(QColor{Qt::black})));
    }
  }
  else
  {
    prev--;
    new_grad.insert(std::make_pair(np, color_or(prev_color)));
  }
  return new_grad;
}

Presenter::Presenter(
    const Gradient::ProcessModel& layer, View* view, const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
{
  putToFront();
  connect(&layer, &ProcessModel::gradientChanged, this, [&] {
    m_view->setGradient(layer.gradient());
  });

  m_view->setGradient(layer.gradient());
  connect(m_view, &View::doubleClicked, this, [&](QPointF pos) {
    auto new_grad = addPointToGradient(layer.gradient(), pos.x() / m_view->dataWidth());
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });
  connect(m_view, &View::dropPoint, this, [&](double pos, QColor c) {
    auto new_grad = addPointToGradient(layer.gradient(), pos / m_view->dataWidth(), &c);
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::movePoint, this, [&](double orig, double cur) {
    auto new_grad = layer.gradient();
    auto it = new_grad.find(orig);
    if(it == new_grad.end())
      return;

    auto col = it->second;
    new_grad.erase(it);
    new_grad.insert(std::make_pair(cur, col));
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::removePoint, this, [&](double orig) {
    auto new_grad = layer.gradient();
    auto it = new_grad.find(orig);
    if(it == new_grad.end())
      return;

    new_grad.erase(it);
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });

  connect(m_view, &View::setColor, this, [&](double pos, QColor col) {
    auto new_grad = layer.gradient();
    auto it = new_grad.find(pos);
    if(it == new_grad.end())
      return;

    it->second = col;
    CommandDispatcher<>{context().context.commandStack}.submit<ChangeGradient>(
        layer, new_grad);
  });
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
  m_view->setDataWidth(m_process.duration().toPixels(m_zoomRatio));
}
}
