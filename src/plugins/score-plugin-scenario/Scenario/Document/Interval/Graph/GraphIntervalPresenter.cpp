#include "GraphIntervalPresenter.hpp"
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <score/graphics/PainterPath.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPen>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::GraphalIntervalPresenter)

static const QPainterPathStroker& cableStroker() {
  static const QPainterPathStroker cable_stroker{[] {
      QPen pen;
      pen.setCapStyle(Qt::PenCapStyle::RoundCap);
      pen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
      pen.setWidthF(9.);
      return pen;
  }()};
  return cable_stroker;
}
namespace Scenario
{

GraphalIntervalPresenter::GraphalIntervalPresenter(
    const IntervalModel& model,
    const StateView& start,
    const StateView& end,
    const Process::Context& ctx,
    QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_model{model}
  , m_start{start}
  , m_end{end}
  , m_context{ctx}
{
  resize();
  connect(&model.selection, &Selectable::changed,
          this, [this] { update(); });
  connect(&model, &IntervalModel::executionStateChanged,
          this, [this] { update(); });
  connect(&model, &IntervalModel::executionStarted,
          this, [this] {
    m_execPing.start();
    update();
  });
}

const Id<IntervalModel>& GraphalIntervalPresenter::id() const { return m_model.id(); }

const IntervalModel& GraphalIntervalPresenter::model() const { return m_model; }

QRectF GraphalIntervalPresenter::boundingRect() const
{
  return cableStroker().createStroke(m_path).boundingRect();
}

void GraphalIntervalPresenter::resize()
{
  prepareGeometryChange();

  clearPainterPath(m_path);
  {
    auto p1 = m_start.pos();
    auto p2 = m_end.pos();

    auto rect = QRectF{p1, p2};
    auto nrect = rect.normalized();
    this->setPos(nrect.topLeft());
    nrect.translate(-nrect.topLeft().x(), -nrect.topLeft().y());

    p1 = mapFromParent(p1);
    p2 = mapFromParent(p2);

    bool x_dir = p1.x() > p2.x();
    auto first = x_dir ? p1 : p2;
    auto last = !x_dir ? p1 : p2;

    int half_length = std::floor(0.5 * (last.x() - first.x()));

    auto y_direction = last.y() > first.y() ? 1 : -1;
    auto offset_y = y_direction * half_length / 10.f;

    m_path.moveTo(first.x(), first.y());
    m_path.cubicTo(
          first.x() + half_length,
          first.y() + offset_y,
          last.x() - half_length,
          last.y() - offset_y,
          last.x(),
          last.y());
  }

  update();
}

const score::Brush& GraphalIntervalPresenter::intervalColor(const Process::Style& skin) noexcept
{
  if (Q_UNLIKELY(m_model.selection.get()))
  {
    return skin.IntervalSelected();
  }
  else if (Q_UNLIKELY(!m_model.consistency.isValid()))
  {
    return skin.IntervalInvalid();
  }
  else if (Q_UNLIKELY(!m_model.consistency.warning()))
  {
    return skin.IntervalWarning();
  }
  else if (Q_UNLIKELY(m_model.executionState() == IntervalExecutionState::Disabled))
  {
    return skin.IntervalInvalid();
  }
  else
  {
    return skin.IntervalBase();
  }
}


void GraphalIntervalPresenter::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = Process::Style::instance();
  auto& brush = this->intervalColor(style);

  painter->setRenderHint(QPainter::Antialiasing, true);

  if(m_execPing.running())
  {
     const auto& nextPen = m_execPing.getNextPen(
           brush.color(),
           style.IntervalPlayFill().color(),
           brush.main.pen2_dotted_square_miter);
     painter->setPen(nextPen);
     update();
  }
  else
  {
    painter->setPen(brush.main.pen2_dotted_square_miter);
  }

  painter->setBrush(style.TransparentBrush());
  painter->drawPath(m_path);
  painter->setRenderHint(QPainter::Antialiasing, false);

  // TODO draw an arrow
}

QPainterPath GraphalIntervalPresenter::shape() const
{
  return cableStroker().createStroke(m_path);
}

QPainterPath GraphalIntervalPresenter::opaqueArea() const
{
  return cableStroker().createStroke(m_path);
}

bool GraphalIntervalPresenter::contains(const QPointF& point) const
{
  return cableStroker().createStroke(m_path).contains(point);
}

void GraphalIntervalPresenter::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  pressed(event->scenePos());
}

void GraphalIntervalPresenter::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  moved(event->scenePos());
}

void GraphalIntervalPresenter::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  released(event->scenePos());
}

}
