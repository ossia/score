// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TemporalIntervalHeader.hpp"

#include "TemporalIntervalPresenter.hpp"

#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QTextLayout>

#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::TemporalIntervalHeader)

namespace Scenario
{
static const constexpr auto interval_header_rack_button_spacing = 17.;
static const constexpr auto interval_header_button_spacing = 20.;

TemporalIntervalHeader::TemporalIntervalHeader(TemporalIntervalPresenter& pres)
    : IntervalHeader{}
    , m_presenter{pres}
    , m_selected{false}
    , m_hovered{false}
    , m_overlay{false}
    , m_executing{false}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setAcceptedMouseButtons(Qt::LeftButton); // needs to be enabled for dblclick
  this->setFlags(
      QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemClipsToShape
      | QGraphicsItem::ItemClipsChildrenToShape | ItemStacksBehindParent);
}

QRectF TemporalIntervalHeader::boundingRect() const
{
  if (Q_UNLIKELY(m_overlay))
    return {0., 0., m_width, qreal(IntervalHeader::headerHeight())};
  else
    return {5., 0., m_width - 10., qreal(IntervalHeader::headerHeight())};
}

void TemporalIntervalHeader::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);

  if (m_rackButton)
    m_rackButton->setState(m_state == State::RackHidden);

  if (m_width < 30)
    return;

  // Header

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header
  const auto text_width = m_textRectCache.width();
  auto view = getView(*this);
  int text_left = view->mapFromScene(mapToScene({5., 0.})).x();
  int text_right = text_left + text_width;
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
  bool moved = false;
  if (std::abs(m_previous_x - x) > 0.1)
  {
    m_previous_x = x;
    moved = true;
  }

  double rack_button_offset = 0.;

  auto& itv = m_presenter.model();
  if (m_rackButton && !itv.processes.empty())
    rack_button_offset += interval_header_rack_button_spacing;

  const auto p = QPointF{
      m_previous_x + rack_button_offset,
      std::round(-1. + (IntervalHeader::headerHeight() - m_textRectCache.height()) / 2.)};

  if (moved)
    updateButtons();

  if (m_selected)
  {
    auto& style = Process::Style::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(style.NoPen());
    painter->setBrush(itv.muted() ? style.MutedIntervalHeaderBackground() : style.SlotHeader());

    painter->drawPolygon(m_poly);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

   painter->drawPixmap(p, m_line);
  // painter->setPen(Qt::red);
  // painter->setBrush(Qt::transparent);
  // painter->drawPath(shape());
}

void TemporalIntervalHeader::updateButtons()
{
  double pos = m_previous_x - 2;
  auto& itv = m_presenter.model();

  if (m_rackButton && !itv.processes.empty())
  {
    m_rackButton->setPos(pos - 3., 0.);
    pos += interval_header_rack_button_spacing;
  }

  pos += m_textRectCache.width() + 5.;
  if (m_mute)
  {
    m_mute->setPos(pos, 0.);
    pos += interval_header_button_spacing;
  }
  if (m_add)
  {
    m_add->setPos(pos, 0.);
    pos += interval_header_button_spacing;
  }
  if (m_speed)
    m_speed->setPos(pos, -1.);

  updateShape();
}

void TemporalIntervalHeader::updateOverlay()
{
  auto& itv = m_presenter.model();

  bool overlayVisible = m_selected || m_hovered;

  bool hadRackButton = m_rackButton;
  bool hadAdd = m_add;
  bool hadMute = m_mute;
  bool hadSpeed = m_speed;

  if (hadSpeed)
    overlayVisible |= bool(m_speed->spinbox);

  bool needsRackButton = !itv.processes.empty();
  bool needsMute = overlayVisible;
  bool needsAdd = overlayVisible;
  bool needsSpeed = overlayVisible && m_executing;

  prepareGeometryChange();

  m_overlay = false;

  const auto& pixmaps = Process::Pixmaps::instance();

  /// Show-hide rack ///
  if (hadRackButton && !needsRackButton)
  {
    delete m_rackButton;
    m_rackButton = nullptr;
  }
  else if (!hadRackButton && needsRackButton)
  {
    m_rackButton = new score::QGraphicsSelectablePixmapToggle{
        pixmaps.roll, pixmaps.roll_selected, pixmaps.unroll, pixmaps.unroll_selected, this};

    connect(m_rackButton, &score::QGraphicsSelectablePixmapToggle::toggled, &m_presenter, [=] {
      ((TemporalIntervalPresenter&)m_presenter).changeRackState();
    });

    m_rackButton->setSelected(overlayVisible);
    m_overlay = true;
  }

  /// Mute ///
  if (hadMute && !needsMute)
  {
    delete m_mute;
    m_mute = nullptr;
  }
  else if (!hadMute && needsMute)
  {
    m_mute = new score::QGraphicsPixmapToggle{pixmaps.muted, pixmaps.unmuted, this};
    if (itv.muted())
      m_mute->toggle();
    connect(m_mute, &score::QGraphicsPixmapToggle::toggled, &m_presenter, [&itv](bool b) {
      ((IntervalModel&)itv).setMuted(b);
    });
    con(itv, &IntervalModel::mutedChanged, m_mute, [=](bool b) { m_mute->setState(b); });
  }

  /// Add process ///
  if (hadAdd && !needsAdd)
  {
    delete m_add;
    m_add = nullptr;
  }
  else if (!hadAdd && needsAdd)
  {
    // Add process
    m_add = new score::QGraphicsPixmapButton{pixmaps.add, pixmaps.add, this};
    connect(m_add, &score::QGraphicsPixmapButton::clicked, this, [this] {
      m_view->requestOverlayMenu({});
    });
  }

  /// Speed slider ///
  if (hadSpeed && !needsSpeed)
  {
    delete m_speed;
    m_speed = nullptr;
  }
  else if (!hadSpeed && needsSpeed)
  {
    auto& durations = const_cast<IntervalDurations&>(itv.duration);
    m_speed = new score::QGraphicsSlider{this};
    m_speed->min = -1.;
    m_speed->max = 5.;
    m_speed->setRect({0., 0., 60., headerHeight() * 0.8});
    m_speed->setValue((durations.speed() - m_speed->min) / (m_speed->max - m_speed->min));
    connect(
        m_speed,
        &score::QGraphicsSlider::sliderMoved,
        this,
        [this, min = m_speed->min, max = m_speed->max, &durations] {
          durations.setSpeed(m_speed->value() * (max - min) + min);
        });
    connect(
        m_speed,
        &score::QGraphicsSlider::sliderReleased,
        this,
        [this, min = m_speed->min, max = m_speed->max, &durations] {
          durations.setSpeed(m_speed->value() * (max - min) + min);
        });
  }

  if (m_speed || m_add || m_mute || m_rackButton)
  {
    updateButtons();
  }

  m_overlay |= overlayVisible;
  update();
}

void TemporalIntervalHeader::setSelected(bool b)
{
  m_selected = b;
  updateOverlay();
  on_textChanged();
}

void TemporalIntervalHeader::setExecuting(bool b)
{
  m_executing = b;
  updateOverlay();
}

void TemporalIntervalHeader::setLabel(const QString& label)
{
  on_textChanged();
}

void TemporalIntervalHeader::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  doubleClicked();
}

void TemporalIntervalHeader::updateShape() noexcept
{
  m_poly.clear();
  const auto text_width = m_textRectCache.width();
  double rack_button_offset = 0.;

  auto& itv = m_presenter.model();
  if (m_rackButton && !itv.processes.empty())
    rack_button_offset += interval_header_rack_button_spacing;

  double buttons_width = rack_button_offset + 5.;

  if (m_mute)
    buttons_width += interval_header_button_spacing;
  if (m_add)
    buttons_width += interval_header_button_spacing;
  if (m_speed)
    buttons_width += m_speed->boundingRect().width();

  m_poly << QPointF{0., qreal(IntervalHeader::headerHeight()) - 0.5} << QPointF{5., 0.}
         << QPointF{m_previous_x + text_width + buttons_width, 0.}
         << QPointF{
                m_previous_x + text_width + buttons_width + 5.,
                qreal(IntervalHeader::headerHeight()) - 0.5};
}

void TemporalIntervalHeader::setState(IntervalHeader::State s)
{
  if (s == m_state)
    return;

  m_state = s;
  on_textChanged();
  updateOverlay();
}

void TemporalIntervalHeader::on_textChanged()
{
  prepareGeometryChange();
  const auto& skin = Process::Style::instance();
  const auto& font = skin.skin.Bold10Pt;
  const auto& model = m_presenter.model().metadata();
  const auto& label = model.getLabel();
  const auto& name = model.getName();
  const auto& text = m_state != State::Hidden ? (label.isEmpty() ? name : label) : label;
  if (text.isEmpty())
  {
    m_textRectCache = {};
    m_line = QPixmap{};
    return;
  }
  else
  {
    QImage img;
    QTextLayout layout(text, font);
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    m_textRectCache = line.naturalTextRect();
    auto r = line.glyphRuns();
    if (r.size() > 0)
    {
      img = newImage(m_textRectCache.width(), m_textRectCache.height());

      QPainter p{&img};
      if (m_hovered || m_selected)
        p.setPen(skin.IntervalBraceSelected());
      else
      {
        const auto& col = model.getColor();
        if (&col.getBrush() == &skin.IntervalDefaultBackground())
          p.setPen(skin.IntervalHeaderTextPen());
        else
          p.setPen(QPen(col.getBrush().color()));
      }
      p.drawGlyphRun(QPointF{0, 0}, r[0]);
    }
    this->m_line = QPixmap::fromImage(std::move(img), Qt::NoFormatConversion);
  }
  updateButtons();
  update();
}

void TemporalIntervalHeader::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  m_hovered = true;
  updateOverlay();
  intervalHoverEnter();
  on_textChanged();
}

void TemporalIntervalHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  m_hovered = false;
  updateOverlay();
  intervalHoverLeave();
  on_textChanged();
}

void TemporalIntervalHeader::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  m_view->dragEnterEvent(event);
  event->accept();
}

void TemporalIntervalHeader::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_view->dragLeaveEvent(event);
  event->accept();
}

void TemporalIntervalHeader::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  m_view->dropEvent(event);
}
}

bool Scenario::TemporalIntervalHeader::contains(const QPointF& point) const
{
  return QGraphicsItem::contains(point);
  /*
  if(!m_selected && !m_hovered)
  {
    bool insmallrect = point.x() > 2. && point.x() < m_width - 2.
        && point.y() > headerHeight() - headerHeight() / 2.;
    return insmallrect || m_textRectCache.adjusted(m_previous_x, 0,
  m_previous_x, 0).contains(point);
  }
  else
  {
    return m_poly.containsPoint(point, Qt::OddEvenFill);
  }
  */
}

QPainterPath Scenario::TemporalIntervalHeader::shape() const
{
  return QGraphicsItem::shape();
  /* TOOD improve it
  if(m_selected || m_hovered)
  {
    return QGraphicsItem::shape();
  }

  QPainterPath p;

  if(m_textRectCache.width() > 0)
  {
    p.addRect(m_textRectCache);
    p.addRect(, headerHeight() / 2., m_width - 4., headerHeight() / 2.);
  }


  return p;
  */
}
