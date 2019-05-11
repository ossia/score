// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TemporalIntervalHeader.hpp"

#include "TemporalIntervalPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPoint>

#include <cmath>
#include <wobjectimpl.h>

#include <algorithm>
W_OBJECT_IMPL(Scenario::RackButton)
W_OBJECT_IMPL(Scenario::TemporalIntervalHeader)

namespace Scenario
{
TemporalIntervalHeader::TemporalIntervalHeader(TemporalIntervalPresenter& pres)
    : IntervalHeader{}
    , m_presenter{pres}
    , m_selected{false}
    , m_hovered{false}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setAcceptedMouseButtons(
      Qt::LeftButton); // needs to be enabled for dblclick
  this->setFlags(
      QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemClipsToShape
      | QGraphicsItem::ItemClipsChildrenToShape);
  this->setZValue(-1);
}

QRectF TemporalIntervalHeader::boundingRect() const
{
  return {0., 0., m_width, qreal(IntervalHeader::headerHeight())};
}

void TemporalIntervalHeader::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);

  if (m_rackButton)
    m_rackButton->setUnrolled(m_state == State::RackHidden);

  if (m_width < 30)
    return;

  // Header

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header
  const auto textWidth = m_textRectCache.width();
  auto view = getView(*this);
  int text_left
      = view->mapFromScene(mapToScene({5., 0.}))
            .x();
  int text_right
      = text_left + textWidth;
  double x = 5.;
  const constexpr double min_x = 5.;
  const double max_x = view->width() - 30.;

  if (text_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - text_left + min_x;
  }
  else if (text_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - text_right + max_x;
  }

  x = std::max(x, 10.);
  if (std::abs(m_previous_x - x) > 0.1)
  {
    m_previous_x = x;
  }
  const auto p = QPointF{
      m_previous_x,
      (IntervalHeader::headerHeight() - m_textRectCache.height()) / 2.};

  if(m_selected)
  {
    auto& style = Process::Style::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(style.NoPen);
    painter->setBrush(style.SlotHeader.getBrush());

    QPolygonF poly;
    poly << QPointF{0., qreal(IntervalHeader::headerHeight()) - 1.5}
         << QPointF{5., 0.}
         << QPointF{m_previous_x + textWidth + 117., 0.}
         << QPointF{m_previous_x + textWidth + 122., qreal(IntervalHeader::headerHeight()) - 1.5};

    painter->drawPolygon(poly);
    painter->setRenderHint(QPainter::Antialiasing, false);
    updateButtons();
  }
  painter->drawImage(p, m_line);
}

void TemporalIntervalHeader::updateButtons()
{
  double pos = m_previous_x + m_textRectCache.width();
  if (m_rackButton)
    m_rackButton->setPos(pos += 12, 0);
  if (m_mute)
    m_mute->setPos(pos += 11, 0);
  if (m_add)
    m_add->setPos(pos += 16, 0);
  if(m_speed)
    m_speed->setPos(pos += 16, headerHeight() * 0.05);
}

void TemporalIntervalHeader::enableOverlay(bool b)
{
  delete m_rackButton;
  m_rackButton = nullptr;

  delete m_add;
  m_add = nullptr;

  delete m_mute;
  m_mute = nullptr;

  delete m_speed;
  m_speed = nullptr;

  if (b)
  {
    auto& itv = m_presenter.model();
    auto& durations = const_cast<IntervalDurations&>(itv.duration);

    // Show-hide rack
    if(!itv.processes.empty())
    {
      m_rackButton = new RackButton{this};
      connect(m_rackButton, &RackButton::clicked, &m_presenter, [=] {
        ((TemporalIntervalPresenter&)m_presenter).changeRackState();
      });
    }

    // Mute
    static const auto pix_unmuted
        = score::get_pixmap(":/icons/process_on.png");
    static const auto pix_muted = score::get_pixmap(":/icons/process_off.png");

    m_mute = new score::QGraphicsPixmapToggle{pix_muted, pix_unmuted, this};
    if (itv.muted())
      m_mute->toggle();
    connect(
        m_mute,
        &score::QGraphicsPixmapToggle::toggled,
        &m_presenter,
        [&itv](bool b) { ((IntervalModel&)itv).setMuted(b); });
    con(itv,
        &IntervalModel::mutedChanged,
        m_mute,
        [=](bool b) { m_mute->setState(b); });

    // Add process
    static const auto pix_add_on = score::get_pixmap(":/icons/process_add_off.png");
    m_add = new score::QGraphicsPixmapButton{pix_add_on, pix_add_on, this};
    connect(m_add, &score::QGraphicsPixmapButton::clicked,
            this, [this] {
      m_view->requestOverlayMenu({});
    });;

    // Speed slider
    m_speed = new score::QGraphicsSlider{this};
    m_speed->min = -1.;
    m_speed->max = 5.;
    m_speed->setRect({0., 0., 60., headerHeight() * 0.8});
    m_speed->setValue((durations.speed() - m_speed->min) / (m_speed->max - m_speed->min));
    connect(m_speed, &score::QGraphicsSlider::sliderMoved,
            this, [this, min=m_speed->min, max=m_speed->max, &durations] {
      durations.setSpeed(m_speed->value() * (max - min) + min);
    });
    connect(m_speed, &score::QGraphicsSlider::sliderReleased,
            this, [this, min=m_speed->min, max=m_speed->max, &durations] {
      durations.setSpeed(m_speed->value() * (max - min) + min);
    });

    updateButtons();
  }
}

void TemporalIntervalHeader::setSelected(bool b)
{
  m_selected = b;
  enableOverlay(m_selected || m_hovered);
  on_textChanged();
}

void TemporalIntervalHeader::setLabel(const QString& label)
{
  on_textChanged();
}

void TemporalIntervalHeader::mouseDoubleClickEvent(
      QGraphicsSceneMouseEvent* event)
{
  doubleClicked();
}

void TemporalIntervalHeader::setState(IntervalHeader::State s)
{
  if (s == m_state)
    return;

  m_state = s;
  on_textChanged();
  update();
}

void TemporalIntervalHeader::on_textChanged()
{
  const auto& skin = Process::Style::instance();
  const auto& font = skin.skin.Bold10Pt;
  const auto& model = m_presenter.model().metadata();
  const auto& label = model.getLabel();
  const auto& name = model.getName();
  const auto& text = m_state != State::Hidden ? (label.isEmpty() ? name : label) : label;
  if (text.isEmpty())
  {
    m_textRectCache = {};
    m_line = QImage{};
    return;
  }
  else
  {
    QTextLayout layout(text, font);
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    m_textRectCache = line.naturalTextRect();
    m_line = QImage{};
    auto r = line.glyphRuns();
    if (r.size() > 0)
    {
      double ratio = 1.;
      if (auto v = getView(*this))
        ratio = v->devicePixelRatioF();
      m_line = QImage(
          m_textRectCache.width() * ratio,
          m_textRectCache.height() * ratio,
          QImage::Format_ARGB32_Premultiplied);
      m_line.setDevicePixelRatio(ratio);
      m_line.fill(Qt::transparent);

      QPainter p{&m_line};
      if(!m_selected)
      {
        const auto& col = model.getColor();
        if(col == skin.IntervalDefaultBackground)
          p.setPen(skin.IntervalHeaderTextPen);
        else
          p.setPen(QPen(model.getColor().getBrush().color()));
      }
      else
        p.setPen(skin.IntervalBraceSelected);

      p.drawGlyphRun(QPointF{0, 0}, r[0]);
    }
  }
}

void TemporalIntervalHeader::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  m_hovered = true;
  enableOverlay(m_selected || m_hovered);
  intervalHoverEnter();
}

void TemporalIntervalHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  m_hovered = false;
  enableOverlay(m_selected || m_hovered);
  intervalHoverLeave();
}

void TemporalIntervalHeader::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  m_view->setDropTarget(true);
  event->accept();
}

void TemporalIntervalHeader::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_view->setDropTarget(m_view->contains(mapToItem(m_view, event->pos())));
  event->accept();
}

void TemporalIntervalHeader::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  m_view->dropEvent(event);
}

RackButton::RackButton(QGraphicsItem* parent)
  : QGraphicsObject{parent}
{
  setCursor(Qt::CrossCursor);
  setTransformOriginPoint(boundingRect().center());
  setRotation(m_unroll ? 0 : 90);
}

void RackButton::setUnrolled(bool b)
{
  if (m_unroll != b)
  {
    m_unroll = b;

    setRotation(m_unroll ? 0 : 90);
    update();
  }
}

static const QPainterPath trianglePath{[] {
  QPainterPath p;
  QPainterPathStroker s;
  s.setCapStyle(Qt::RoundCap);
  s.setJoinStyle(Qt::RoundJoin);
  s.setWidth(2);

  p.addPolygon(
      QVector<QPointF>{QPointF(0, 5), QPointF(0, 21), QPointF(9, 13)});
  p.closeSubpath();
  p = QTransform().scale(0.8, 0.8).map(p);

  return p + s.createStroke(p);
}()};
static const auto rotatedTriangle
    = QTransform().rotate(90).translate(8, -12).map(trianglePath);

static const QPainterPath arrowPath{[] {
  QPainterPath p;

  p.moveTo(QPointF(5, 5));
  p.lineTo(QPointF(14, 13));
  p.lineTo(QPointF(5, 21));

  p = QTransform().scale(0.55, 0.55).translate(0, 6).map(p);

  return p;
}()};
static const auto rotatedArrow
    = QTransform().rotate(90).translate(6, -16).map(arrowPath);

QRectF RackButton::boundingRect() const
{
  return QRectF(trianglePath.boundingRect());
}

void RackButton::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(Qt::NoBrush);

  auto pen = QPen(QColor("#f6a019"));
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  pen.setWidth(2);

  if (m_unroll)
  {
    pen.setColor("#a0a0a0");
    painter->setPen(pen);
  }
  else
  {
    painter->setPen(pen);
  }
  painter->drawPath(arrowPath);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void RackButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  update();
  clicked();
}

void RackButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void RackButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  update();
}
}
