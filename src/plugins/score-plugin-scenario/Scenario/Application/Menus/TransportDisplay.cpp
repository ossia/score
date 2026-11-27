#include "TransportDisplay.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/model/Skin.hpp>

#include <ossia/detail/flicks.hpp>

#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

#include <array>

namespace Scenario
{
namespace
{
//! Space between the digits and the pulse grid.
constexpr int grid_spacing = 8;
//! Space between the bars and the time under them.
constexpr int line_spacing = 4;

QString formatTime(int64_t ms)
{
  const int64_t hours = ms / 3600000;
  ms -= hours * 3600000;
  const int64_t minutes = ms / 60000;
  ms -= minutes * 60000;
  const int64_t seconds = ms / 1000;
  ms -= seconds * 1000;

  return QStringLiteral("%1:%2:%3.%4")
      .arg(hours, 2, 10, QLatin1Char('0'))
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(seconds, 2, 10, QLatin1Char('0'))
      .arg(ms, 3, 10, QLatin1Char('0'));
}

QString formatBars(const MusicalPosition& pos)
{
  return QStringLiteral("%1.%2.%3")
      .arg(pos.bar + 1)
      .arg(pos.pulse + 1)
      .arg(pos.subdivision + 1);
}
}

TransportDisplay::TransportDisplay(QWidget* parent)
    : QWidget{parent}
{
  setObjectName("TimeLabel");

  applySkin();
  connect(
      &score::Skin::instance(), &score::Skin::changed, this, [this] { applySkin(); });

  setPosition(TimeVal::zero(), nullptr, false);
}

TransportDisplay::~TransportDisplay() = default;

std::vector<std::function<void(QMenu&)>>& TransportDisplay::menuEntries()
{
  static std::vector<std::function<void(QMenu&)>> entries;
  return entries;
}

bool TransportDisplay::showMenu()
{
  if(menuEntries().empty())
    return false;
  QMenu menu{this};
  menu.setToolTipsVisible(true);
  for(auto& f : menuEntries())
    f(menu);
  if(menu.isEmpty())
    return false;
  // Above the display, which sits at the bottom of the window
  const QSize size = menu.sizeHint();
  menu.exec(mapToGlobal(QPoint{0, -size.height()}));
  return true;
}

void TransportDisplay::contextMenuEvent(QContextMenuEvent* event)
{
  if(showMenu())
    event->accept();
  else
    QWidget::contextMenuEvent(event);
}

void TransportDisplay::mousePressEvent(QMouseEvent* event)
{
  if(event->button() == Qt::LeftButton && showMenu())
    event->accept();
  else
    QWidget::mousePressEvent(event);
}

void TransportDisplay::setMusical(bool b)
{
  if(m_musical == b)
    return;
  m_musical = b;
  update();
}

void TransportDisplay::applySkin()
{
  auto& skin = score::Skin::instance();
  m_timeFont = skin.TimecodeFont;
  m_barsFont = skin.TimecodeFont;

  // Half the size, in the regular weight: the demi-bold of the main digits
  // clogs at this size.
  m_smallFont = skin.TimecodeFont;
  const int px = m_timeFont.pixelSize() > 0 ? m_timeFont.pixelSize()
                                            : QFontMetrics{m_timeFont}.height();
  m_smallFont.setPixelSize(std::max(9, px / 2));
  m_smallFont.setWeight(QFont::Medium);

  // Room for the two lines of the musical mode in both modes.
  m_size = QFontMetrics{m_timeFont}.boundingRect("0000:00:00.000").size();
  m_size.setHeight(std::max(m_size.height(), contentHeight() + 2));

  // see QWidgetPrivate::updateGeometry_helper
  setMinimumSize(m_size);
  setMaximumSize(m_size);
  updateGeometry();

  layout();
  update();
}

int TransportDisplay::contentHeight() const
{
  // Digits have no descender: the two lines are stacked by their cap heights.
  return QFontMetrics{m_barsFont}.capHeight() + line_spacing
         + QFontMetrics{m_smallFont}.capHeight();
}

void TransportDisplay::layout()
{
  const int bars_h = QFontMetrics{m_barsFont}.capHeight();
  const int small_h = QFontMetrics{m_smallFont}.capHeight();
  const int content = contentHeight();
  const int top = std::max(0, (height() - content) / 2);

  m_barsBaseline = top + bars_h;
  m_smallBaseline = m_barsBaseline + line_spacing + small_h;

  const int side = std::min(height(), content);
  m_gridRect = QRect{width() - side, top, side, side};
  m_textRect = QRect{0, 0, width() - side - grid_spacing, height()};
}

void TransportDisplay::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  layout();
}

void TransportDisplay::setPosition(
    TimeVal date, const IntervalModel* root, bool running)
{
  const int64_t ms = date.impl / ossia::flicks_per_millisecond<int64_t>;
  bool text_changed = false;
  if(ms != m_milliseconds)
  {
    m_milliseconds = ms;
    m_timeText = formatTime(ms);
    text_changed = true;
  }

  // Kept up to date in time mode too: switching modes shows the position at
  // once, without waiting for the next tick.
  static const std::array<std::pair<TimeVal, ossia::time_signature>, 0> no_signature;
  const auto pos = root ? musicalPosition(root->timeSignatureMap(), date)
                        : musicalPosition(no_signature, date);

  bool grid_changed = running != m_running;
  if(!m_hasPosition || pos != m_position)
  {
    grid_changed |= !m_hasPosition || pos.pulse != m_position.pulse
                    || pos.signature.upper != m_position.signature.upper;
    m_position = pos;
    m_hasPosition = true;
    m_barsText = formatBars(pos);
    text_changed |= m_musical;
  }
  m_running = running;

  if(!m_musical)
  {
    if(text_changed)
      update();
    return;
  }

  if(text_changed)
    update(m_textRect);
  if(grid_changed)
    update(m_gridRect);
}

void TransportDisplay::paintEvent(QPaintEvent* event)
{
  QPainter p{this};
  const QColor text = palette().color(QPalette::WindowText);

  if(!m_musical)
  {
    p.setFont(m_timeFont);
    p.setPen(text);
    p.drawText(rect(), Qt::AlignRight | Qt::AlignVCenter, m_timeText);
    return;
  }

  if(event->rect().intersects(m_textRect))
  {
    const int right = m_textRect.right() + 1;

    p.setFont(m_barsFont);
    p.setPen(text);
    p.drawText(
        right - QFontMetrics{m_barsFont}.horizontalAdvance(m_barsText), m_barsBaseline,
        m_barsText);

    p.setFont(m_smallFont);
    p.setPen(score::Skin::instance().Gray.color());
    p.drawText(
        right - QFontMetrics{m_smallFont}.horizontalAdvance(m_timeText),
        m_smallBaseline, m_timeText);
  }

  if(event->rect().intersects(m_gridRect))
    paintGrid(p);
}

void TransportDisplay::paintGrid(QPainter& p) const
{
  auto& skin = score::Skin::instance();
  const auto grid = pulseGridLayout(m_position.signature.upper);

  // Square cells on whole pixels. Past the point where a gap would take as
  // much room as a cell, the cells touch.
  const int side = m_gridRect.width();
  int gap = 2;
  int cell = (side - gap * (grid.columns - 1)) / grid.columns;
  if(cell < 4)
  {
    gap = 1;
    cell = (side - gap * (grid.columns - 1)) / grid.columns;
  }
  if(cell < 2)
  {
    gap = 0;
    cell = std::max(1, side / grid.columns);
  }

  const int w = grid.columns * cell + (grid.columns - 1) * gap;
  const int h = grid.rows * cell + (grid.rows - 1) * gap;
  const int x0 = m_gridRect.x() + (side - w) / 2;
  const int y0 = m_gridRect.y() + (m_gridRect.height() - h) / 2;

  // The downbeat is in the accent colour whatever its state, the other pulses
  // in grays: lit while they sound, dimmed once played, dark before.
  const int current = m_running ? m_position.pulse : -1;
  for(int i = 0; i < grid.pulses; i++)
  {
    const QRect r{
        x0 + grid.column(i) * (cell + gap), y0 + grid.row(i) * (cell + gap), cell, cell};
    if(i == 0)
    {
      p.fillRect(r, skin.DarkGray.main.brush);
      p.setOpacity(i == current ? 1. : current > 0 ? 0.6 : 0.3);
      p.fillRect(r, skin.Base4.main.brush);
      p.setOpacity(1.);
    }
    else if(i == current)
      p.fillRect(r, skin.Light.main.brush);
    else if(i < current)
      p.fillRect(r, skin.Gray.main.brush);
    else
      p.fillRect(r, skin.DarkGray.main.brush);
  }
}
}
