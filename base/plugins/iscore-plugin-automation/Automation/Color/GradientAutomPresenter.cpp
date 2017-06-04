#include <Automation/Color/GradientAutomPresenter.hpp>
#include <Automation/Color/GradientAutomModel.hpp>
#include <Automation/Color/GradientAutomView.hpp>

#include <Process/Focus/FocusDispatcher.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <ossia/detail/math.hpp>
namespace Gradient
{
Presenter::Presenter(
    const Gradient::ProcessModel& layer,
    View* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : LayerPresenter{ctx, parent}
    , m_layer{layer}
    , m_view{view}
{
  putToFront();
  connect(&m_layer, &ProcessModel::gradientChanged, this, [&] {
    m_view->setGradient(m_layer.gradient());
  });

  m_view->setGradient(m_layer.gradient());
  connect(m_view, &View::doubleClicked, this, [&] (QPointF pos) {
    auto np = pos.x() / m_view->width();
    auto new_grad = m_layer.gradient();
    auto prev = new_grad.lower_bound(np);
    if(prev == new_grad.begin())
      return;
    if(prev == new_grad.end())
      prev = new_grad.begin();
    else
      prev--;

    new_grad.insert(std::make_pair(np, prev->second));
    CommandDispatcher<>{context().context.commandStack}
          .submitCommand<ChangeGradient>(layer, new_grad);
  });

  connect(m_view, &View::movePoint, this,
          [&] (double orig, double cur) {
    auto new_grad = m_layer.gradient();
    auto it = new_grad.find(orig);
    if(it == new_grad.end())
      return;

    auto col = it->second;
    new_grad.erase(it);
    new_grad.insert(std::make_pair(cur, col));
    CommandDispatcher<>{context().context.commandStack}
      .submitCommand<ChangeGradient>(layer, new_grad);
  });

  connect(m_view, &View::removePoint, this,
          [&] (double orig) {
    auto new_grad = m_layer.gradient();
    auto it = new_grad.find(orig);
    if(it == new_grad.end())
      return;

    new_grad.erase(it);
    CommandDispatcher<>{context().context.commandStack}
      .submitCommand<ChangeGradient>(layer, new_grad);
  });

  connect(m_view, &View::setColor, this,
          [&] (double pos, QColor col) {
    auto new_grad = m_layer.gradient();
    auto it = new_grad.find(pos);
    if(it == new_grad.end())
      return;

    it->second = col;
    CommandDispatcher<>{context().context.commandStack}
      .submitCommand<ChangeGradient>(layer, new_grad);
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
