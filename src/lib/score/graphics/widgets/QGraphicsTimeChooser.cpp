#include "QGraphicsTimeChooser.hpp"

#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsTimeChooser)
namespace score
{
namespace
{
struct Division
{
  float fraction; // of a whole note
  const char* label;
  bool straight; // reachable with a plain drag; dotted / triplet need Alt or Shift
};

// Sorted by increasing duration so dragging right always means "longer",
// like in free mode. The twelve legacy ratios are all present so existing
// documents round-trip exactly.
constexpr Division divisions[] = {
    {1.f / 64.f, "1/64", true},   {1.f / 48.f, "1/32T", false},
    {1.f / 32.f, "1/32", true},   {1.f / 24.f, "1/16T", false},
    {3.f / 64.f, "1/32.", false}, {1.f / 16.f, "1/16", true},
    {1.f / 12.f, "1/8T", false},  {3.f / 32.f, "1/16.", false},
    {1.f / 8.f, "1/8", true},     {1.f / 6.f, "1/4T", false},
    {3.f / 16.f, "1/8.", false},  {1.f / 4.f, "1/4", true},
    {1.f / 3.f, "1/2T", false},   {3.f / 8.f, "1/4.", false},
    {1.f / 2.f, "1/2", true},     {2.f / 3.f, "1/1T", false},
    {3.f / 4.f, "1/2.", false},   {1.f, "1/1", true},
};
constexpr int division_count = std::ssize(divisions);
constexpr int straight_indices[] = {0, 2, 5, 8, 11, 14, 17};
constexpr int straight_count = std::ssize(straight_indices);

int nearestDivision(float frac) noexcept
{
  int best = 0;
  float bestDist = std::abs(divisions[0].fraction - frac);
  for(int i = 1; i < division_count; i++)
  {
    const float d = std::abs(divisions[i].fraction - frac);
    if(d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best;
}
}

QGraphicsTimeChooser::QGraphicsTimeChooser(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
  m_syncIndex = m_lastSyncIndex = 8; // 1/8th
}

QGraphicsTimeChooser::~QGraphicsTimeChooser() { }

void QGraphicsTimeChooser::setRange(double min, double max, double init)
{
  this->min = min;
  this->max = max;
  this->init = init;
  update();
}

void QGraphicsTimeChooser::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  update();
}

void QGraphicsTimeChooser::setValue(ossia::vec2f v)
{
  m_sync = v[1] != 0.f;
  if(m_sync)
    m_syncIndex = m_lastSyncIndex = nearestDivision(v[0]);
  else
    m_value01 = m_lastFree01 = ossia::clamp(double(v[0]), 0., 1.);
  update();
}

ossia::vec2f QGraphicsTimeChooser::value() const noexcept
{
  if(m_sync)
    return {divisions[m_syncIndex].fraction, 1.f};
  else
    return {float(m_value01), 0.f};
}

void QGraphicsTimeChooser::setExecutionValue(ossia::vec2f v)
{
  m_hasExec = true;
  m_execSync = v[1] != 0.f;
  if(m_execSync)
    m_execX = nearestDivision(v[0]) / float(division_count - 1);
  else
    m_execX = ossia::clamp(v[0], 0.f, 1.f);
  update();
}

void QGraphicsTimeChooser::resetExecution()
{
  m_hasExec = false;
  update();
}

void QGraphicsTimeChooser::syncChanged(bool sync)
{
  if(sync == m_sync)
    return;

  // Per-mode memory: come back to where that mode was left
  if(m_sync)
  {
    m_lastSyncIndex = m_syncIndex;
    m_value01 = m_lastFree01;
  }
  else
  {
    m_lastFree01 = m_value01;
    m_syncIndex = m_lastSyncIndex;
  }
  m_sync = sync;

  sliderMoved();
  sliderReleased();
  update();
}

QRectF QGraphicsTimeChooser::boundingRect() const
{
  return m_rect;
}

QString QGraphicsTimeChooser::freeText() const
{
  const double secs = min + m_value01 * (max - min);
  if(secs < 1.)
    return QString::number(secs * 1000., 'f', 0) + QStringLiteral(" ms");
  else if(secs < 10.)
    return QString::number(secs, 'f', 2) + QStringLiteral(" s");
  else
    return QString::number(secs, 'f', 1) + QStringLiteral(" s");
}

void QGraphicsTimeChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);

  const QRectF bar{m_rect.x() + 1., m_rect.y() + 1., m_rect.width() - 2., 7.};

  // Bar background
  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Emphasis2.main.brush);
  painter->drawRoundedRect(bar, 1, 1);

  // Fill up to the current value
  const double pos01
      = m_sync ? m_syncIndex / double(division_count - 1) : m_value01;
  painter->setBrush(skin.Base4.main.brush);
  painter->drawRect(
      QRectF{bar.x(), bar.y(), std::round(pos01 * bar.width()), bar.height()});

  // In sync mode, tick marks on the straight divisions
  if(m_sync)
  {
    painter->setPen(skin.Background2.main.pen1);
    for(int i : straight_indices)
    {
      const double x
          = std::round(bar.x() + bar.width() * i / double(division_count - 1));
      painter->drawLine(QPointF{x, bar.y() + 1.}, QPointF{x, bar.bottom() - 1.});
    }
  }

  // Execution feedback: a thin underline below the bar
  if(m_hasExec)
  {
    painter->fillRect(
        QRectF{bar.x(), bar.bottom() + 1., m_execX * bar.width(), 1.},
        skin.Base4.lighter180.brush);
  }

  // Readout row: value text; a small note glyph marks sync mode
#if defined(__linux__)
  // widget is null when painting through QGraphicsScene::render
  static const auto dpi_adjust
      = (widget ? widget->devicePixelRatioF() : painter->device()->devicePixelRatioF())
                > 1
            ? 0
            : -1;
#elif defined(_WIN32)
  static const constexpr auto dpi_adjust = 0;
#else
  static const constexpr auto dpi_adjust = -2;
#endif
  painter->setPen(skin.Base4.lighter180.pen1);
  painter->setFont(skin.Medium8Pt);
  const auto textrect
      = m_rect.adjusted(2., bar.height() + 3. + dpi_adjust, -2., -1.);
  const QString text
      = m_sync ? QString::fromLatin1(divisions[m_syncIndex].label) : freeText();
  painter->drawText(textrect, text, QTextOption(Qt::AlignCenter));

  if(m_sync)
  {
    // Note glyph: filled head + stem, to the left of the text
    const auto tw = painter->fontMetrics().horizontalAdvance(text);
    const double gx
        = std::round(textrect.center().x() - tw / 2. - 8.);
    const double gy = std::round(textrect.center().y());
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(skin.NoPen);
    painter->setBrush(skin.Base4.lighter180.brush);
    painter->drawEllipse(QRectF{gx, gy + 1., 3.5, 2.75});
    painter->setPen(skin.Base4.lighter180.pen1);
    painter->drawLine(QPointF{gx + 3.5, gy + 2.} , QPointF{gx + 3.5, gy - 4.});
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
}

void QGraphicsTimeChooser::dragTo(double posX, bool fullTable)
{
  const double t
      = ossia::clamp((posX - m_rect.x() - 1.) / (m_rect.width() - 2.), 0., 1.);
  if(!m_sync)
  {
    if(t != m_value01)
    {
      m_value01 = m_lastFree01 = t;
      sliderMoved();
      update();
    }
  }
  else
  {
    int idx;
    if(fullTable)
    {
      idx = int(std::lround(t * (division_count - 1)));
    }
    else
    {
      const int s = int(std::lround(t * (straight_count - 1)));
      idx = straight_indices[s];
    }
    if(idx != m_syncIndex)
    {
      m_syncIndex = m_lastSyncIndex = idx;
      sliderMoved();
      update();
    }
  }
}

void QGraphicsTimeChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const bool inBar = event->pos().y() < m_rect.y() + 10.;
  if(inBar)
  {
    m_grab = true;
    dragTo(
        event->pos().x(),
        bool(qGuiApp->keyboardModifiers() & (Qt::AltModifier | Qt::ShiftModifier)));
  }
  else
  {
    syncChanged(!m_sync);
  }
  event->accept();
}

void QGraphicsTimeChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
    dragTo(
        event->pos().x(),
        bool(qGuiApp->keyboardModifiers() & (Qt::AltModifier | Qt::ShiftModifier)));
  event->accept();
}

void QGraphicsTimeChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    dragTo(
        event->pos().x(),
        bool(qGuiApp->keyboardModifiers() & (Qt::AltModifier | Qt::ShiftModifier)));
    m_grab = false;
    sliderReleased();
  }
  event->accept();
}
}
