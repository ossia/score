// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TemporalIntervalHeader.hpp"

#include "TemporalIntervalPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
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
    : IntervalHeader{}, m_presenter{pres}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptDrops(true);
  this->setAcceptedMouseButtons(
      Qt::LeftButton); // needs to be enabled for dblclick
  this->setFlags(
      QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemClipsToShape
      | QGraphicsItem::ItemClipsChildrenToShape);
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

  if (m_button)
    m_button->setUnrolled(m_state == State::RackHidden);

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
      = view->mapFromScene(mapToScene({5. + textWidth , 0.}))
            .x();
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

  if(m_hasFocus && m_button)
  {
    auto& style = Process::Style::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(style.NoPen);
    painter->setBrush(style.SlotHeader.getBrush());

    QPolygonF poly;
    poly << QPointF{0., qreal(IntervalHeader::headerHeight()) - 1.5}
         << QPointF{5., 0.}
         << QPointF{m_previous_x + textWidth + 47., 0.}
         << QPointF{m_previous_x + textWidth + 52., qreal(IntervalHeader::headerHeight()) - 1.5};

    painter->drawPolygon(poly);
    painter->setRenderHint(QPainter::Antialiasing, false);
    updateButtons();
  }
  painter->drawImage(p, m_line);
}

void TemporalIntervalHeader::updateButtons()
{
  if (m_button)
    m_button->setPos(m_previous_x + m_textRectCache.width() + 10, 0);
  if (m_mute)
    m_mute->setPos(m_previous_x + m_textRectCache.width() + 22, 0);
}

void TemporalIntervalHeader::enableOverlay(bool b)
{
  if (b && m_state != State::Hidden)
  {
    m_button = new RackButton{this};
    connect(m_button, &RackButton::clicked, &m_presenter, [=] {
      ((TemporalIntervalPresenter&)m_presenter).changeRackState();
    });

    static const auto pix_unmuted
        = score::get_pixmap(":/icons/process_on.png");
    static const auto pix_muted = score::get_pixmap(":/icons/process_off.png");

    m_mute = new score::QGraphicsPixmapToggle{pix_muted, pix_unmuted, this};
    if (m_presenter.model().muted())
      m_mute->toggle();
    connect(
        m_mute,
        &score::QGraphicsPixmapToggle::toggled,
        &m_presenter,
        [=](bool b) { ((IntervalModel&)m_presenter.model()).setMuted(b); });
    con(m_presenter.model(),
        &IntervalModel::mutedChanged,
        m_mute,
        [=](bool b) { m_mute->setState(b); });
    updateButtons();
  }
  else
  {
    delete m_button;
    m_button = nullptr;

    delete m_mute;
    m_mute = nullptr;
  }
}

void TemporalIntervalHeader::setFocused(bool b)
{
  m_hasFocus = b;
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
  //
  //if (m_state == State::Hidden)
  //  show();
  //else if (s == State::Hidden)
  //  hide();
  //
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
      if(!m_hasFocus)
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
  intervalHoverEnter();
}

void TemporalIntervalHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  intervalHoverLeave();
}

void TemporalIntervalHeader::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragEnterEvent(event);
  event->accept();
}

void TemporalIntervalHeader::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  event->accept();
}

void TemporalIntervalHeader::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());

  event->accept();
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
