// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TemporalIntervalHeader.hpp"

#include "TemporalIntervalPresenter.hpp"

#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/graphics/ItemBounder.hpp>
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
  if(Q_UNLIKELY(m_overlay))
    return {0., 0., m_width, qreal(IntervalHeader::headerHeight())};
  else
    return {5., 0., m_width - 10., qreal(IntervalHeader::headerHeight())};
}

void TemporalIntervalHeader::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);

  if(m_rackButton)
    m_rackButton->setState(m_state == State::RackHidden);

  if(m_width < 30)
    return;

  // Header

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header

  auto [x, moved] = m_bounder.bound(this, 5., m_textRectCache.width());
  double rack_button_offset = 0.;

  auto& itv = m_presenter.model();
  if(m_rackButton && !itv.processes.empty())
    rack_button_offset += interval_header_rack_button_spacing;

  const auto p = QPointF{
      m_bounder.x() + rack_button_offset,
      std::round(
          -1. + (IntervalHeader::headerHeight() - m_textRectCache.height()) / 2.)};

  if(moved)
    updateButtons();

  if(m_selected)
  {
    auto& style = Process::Style::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(style.NoPen());
    painter->setBrush(
        itv.muted() ? style.MutedIntervalHeaderBackground() : style.SlotHeader());

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
  double pos = m_bounder.x() - 2;
  auto& itv = m_presenter.model();

  if(m_rackButton && !itv.processes.empty())
  {
    m_rackButton->setPos(pos - 3., 0.);
    pos += interval_header_rack_button_spacing;
  }

  pos += m_textRectCache.width() + 5.;
  if(m_mute)
  {
    m_mute->setPos(pos, 0.);
    pos += interval_header_button_spacing;
  }
  if(m_play)
  {
    m_play->setPos(pos, 0.);
    pos += interval_header_button_spacing;
  }
  if(m_stop)
  {
    m_stop->setPos(pos, 0.);
    pos += interval_header_button_spacing;
  }
  if(m_speed)
    m_speed->setPos(pos, -1.);

  updateShape();
}

void TemporalIntervalHeader::updateOverlay()
{
  auto& itv = m_presenter.model();
  auto grand_parent = Scenario::closestParentInterval(itv.parent());

  bool grand_parent_executing = grand_parent && grand_parent->executing();
  bool overlayVisible = m_selected || m_hovered;

  bool hadRackButton = m_rackButton;
  bool hadMute = m_mute;
  bool hadPlay = m_play;
  bool hadStop = m_stop;
  bool hadSpeed = m_speed;

  if(hadSpeed)
    overlayVisible |= bool(m_speed->impl->spinbox);

  bool needsRackButton = !itv.processes.empty();
  bool needsMute = overlayVisible;
  bool needsPlay = overlayVisible && grand_parent_executing;
  bool needsStop = overlayVisible && grand_parent_executing;
  bool needsSpeed = overlayVisible && m_executing;

  prepareGeometryChange();

  m_overlay = false;

  const auto& pixmaps = Process::Pixmaps::instance();

  /// Show-hide rack ///
  if(hadRackButton && !needsRackButton)
  {
    delete m_rackButton;
    m_rackButton = nullptr;
  }
  else if(!hadRackButton && needsRackButton)
  {
    m_rackButton = new score::QGraphicsSelectablePixmapToggle{
        pixmaps.roll, pixmaps.roll_selected, pixmaps.unroll, pixmaps.unroll_selected,
        this};
    m_rackButton->setToolTip(
        tr("Rack folding\nUnroll or re-roll the processes in this interval."));

    connect(
        m_rackButton, &score::QGraphicsSelectablePixmapToggle::toggled, &m_presenter,
        [=] { ((TemporalIntervalPresenter&)m_presenter).changeRackState(); });

    m_rackButton->setSelected(overlayVisible);
    m_overlay = true;
  }

  /// Mute ///
  if(hadMute && !needsMute)
  {
    delete m_mute;
    m_mute = nullptr;
  }
  else if(!hadMute && needsMute)
  {
    m_mute = new score::QGraphicsPixmapToggle{pixmaps.muted, pixmaps.unmuted, this};
    m_mute->setToolTip(
        tr("Mute\nMute the audio output of this interval. Processes will still execute "
           "but will not produce output."));

    if(itv.muted())
      m_mute->toggle();
    connect(
        m_mute, &score::QGraphicsPixmapToggle::toggled, &m_presenter,
        [&itv](bool b) { ((IntervalModel&)itv).setMuted(b); });
    con(itv, &IntervalModel::mutedChanged, m_mute, [=](bool b) { m_mute->setState(b); });
  }

  /// Play ///
  if(hadPlay && !needsPlay)
  {
    delete m_play;
    m_play = nullptr;
  }
  else if(!hadPlay && needsPlay)
  {
    m_play = new score::QGraphicsPixmapButton{
        pixmaps.interval_play, pixmaps.interval_play, this};
    m_play->setToolTip(
        tr("Play\nPlay this interval. Playback follows quantization rules."));
    connect(m_play, &score::QGraphicsPixmapButton::clicked, this, [this] {
      auto& ctx = this->m_presenter.context()
                      .app.guiApplicationPlugin<ScenarioApplicationPlugin>();
      ctx.execution().playInterval(
          const_cast<IntervalModel*>(&this->m_presenter.model()));
    });
  }
  if(hadStop && !needsStop)
  {
    delete m_stop;
    m_stop = nullptr;
  }
  else if(!hadStop && needsStop)
  {
    m_stop = new score::QGraphicsPixmapButton{
        pixmaps.interval_stop, pixmaps.interval_stop, this};
    m_stop->setToolTip(
        tr("Stop\nStop this interval. Stopping follows quantization rules."));
    connect(m_stop, &score::QGraphicsPixmapButton::clicked, this, [this] {
      auto& ctx = this->m_presenter.context()
                      .app.guiApplicationPlugin<ScenarioApplicationPlugin>();
      ctx.execution().stopInterval(
          const_cast<IntervalModel*>(&this->m_presenter.model()));
    });
  }

  /// Speed slider ///
  if(hadSpeed && !needsSpeed)
  {
    delete m_speed;
    m_speed = nullptr;
  }
  else if(!hadSpeed && needsSpeed)
  {
    auto& durations = const_cast<IntervalDurations&>(itv.duration);
    m_speed = new score::QGraphicsSlider{this};
    m_speed->setToolTip(
        tr("Speed control\nChange the playback speed of this interval."));
    m_speed->min = -1.;
    m_speed->max = 5.;
    m_speed->setRect({0., 0., 60., headerHeight() * 0.8});
    m_speed->setValue(
        (durations.speed() - m_speed->min) / (m_speed->max - m_speed->min));
    connect(
        m_speed, &score::QGraphicsSlider::sliderMoved, this,
        [this, min = m_speed->min, max = m_speed->max, &durations] {
      durations.setSpeed(m_speed->value() * (max - min) + min);
        });
    connect(
        m_speed, &score::QGraphicsSlider::sliderReleased, this,
        [this, min = m_speed->min, max = m_speed->max, &durations] {
      durations.setSpeed(m_speed->value() * (max - min) + min);
        });
  }

  if(m_speed || m_play || m_stop || m_mute || m_rackButton)
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
  if(m_rackButton && !itv.processes.empty())
    rack_button_offset += interval_header_rack_button_spacing;

  double buttons_width = rack_button_offset + 5.;

  if(m_mute)
    buttons_width += interval_header_button_spacing;
  if(m_play)
    buttons_width += interval_header_button_spacing;
  if(m_stop)
    buttons_width += interval_header_button_spacing;
  if(m_speed)
    buttons_width += m_speed->boundingRect().width();

  m_poly << QPointF{0., qreal(IntervalHeader::headerHeight()) - 0.5} << QPointF{5., 0.}
         << QPointF{m_bounder.x() + text_width + buttons_width, 0.}
         << QPointF{
                m_bounder.x() + text_width + buttons_width + 5.,
                qreal(IntervalHeader::headerHeight()) - 0.5};
}

void TemporalIntervalHeader::setState(IntervalHeader::State s)
{
  if(s == m_state)
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
  const auto& text = (m_state != State::Hidden || m_overlay)
                         ? (label.isEmpty() ? name : label)
                         : label;
  if(text.isEmpty())
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
    if(r.size() > 0)
    {
      img = newImage(m_textRectCache.width(), m_textRectCache.height());

      QPainter p{&img};
      if(m_hovered || m_selected)
        p.setPen(skin.IntervalBraceSelected());
      else
      {
        const auto& col = model.getColor();
        if(&col.getBrush() == &skin.IntervalDefaultBackground())
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
  //m_view->dragEnterEvent(event);
  event->ignore();
}

void TemporalIntervalHeader::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  //m_view->dragLeaveEvent(event);
  event->ignore();
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
  /* TODO improve it
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
