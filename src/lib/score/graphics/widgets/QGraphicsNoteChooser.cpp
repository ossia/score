#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsNoteChooser.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
namespace score
{
namespace
{
constexpr double pixelsPerNote = 10.;

double dragValue(QGraphicsSceneMouseEvent* event, int min, int max) noexcept
{
  InfiniteScroller::move_free(event);

  double v = InfiniteScroller::origValue - InfiniteScroller::currentDelta / pixelsPerNote;
  if(v <= min)
  {
    InfiniteScroller::currentDelta = (InfiniteScroller::origValue - min) * pixelsPerNote;
    v = min;
  }
  else if(v >= max)
  {
    InfiniteScroller::currentDelta = (InfiniteScroller::origValue - max) * pixelsPerNote;
    v = max;
  }
  return v;
}
}

QRectF QGraphicsNoteChooser::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

QGraphicsNoteChooser::QGraphicsNoteChooser(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorScaleV);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsNoteChooser::setValue(int v)
{
  switch(v)
  {
    case 255:
      m_value = -1;
      break;
    case 254:
      m_value = -2;
      break;
    default:
      m_value = ossia::clamp(v, m_min, m_max);
      break;
  }

  update();
}

int QGraphicsNoteChooser::value() const
{
  return m_value;
}

void QGraphicsNoteChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto srect = boundingRect();
  if(srect.contains(event->pos()))
  {
    m_grab = true;
    m_curValue = m_value;
    InfiniteScroller::start(*this, m_value);
  }

  event->accept();
}

void QGraphicsNoteChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    m_curValue = dragValue(event, m_min, m_max);

    if(int res = std::round(m_curValue); res != m_value)
    {
      m_value = res;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsNoteChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    m_curValue = dragValue(event, m_min, m_max);
    if(int res = std::round(m_curValue); res != m_value)
    {
      m_value = res;
      update();
    }

    InfiniteScroller::stop(*this, event);

    m_grab = false;
    sliderReleased();

    setCursor(score::Skin::instance().CursorScaleV);
  }
  event->accept();
}

static QString noteText(int n)
{
  static constexpr QStringView lit[12]{u"C",  u"C#", u"D",  u"D#", u"E",  u"F",
                                       u"F#", u"G",  u"G#", u"A",  u"A#", u"B"};
  switch(n)
  {
    case -1:
      return "AC";
    case -2:
      return "SL";
    default:
      return QString{"%1%2"}.arg(lit[n % 12]).arg(n / 12 - 1);
  }
}

void QGraphicsNoteChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = score::Skin::instance();
  //const QPen& text = style.Gray.main.pen1;
  const QFont& textFont = style.MonoFontSmall;
  const QPen& currentText = style.Base4.lighter180.pen1;
  // const QBrush& bg = style.SliderBrush;
  // const QPen& noPen = style.NoPen;

  painter->setFont(textFont);
  //painter->setPen(noPen);
  //painter->setBrush(bg);

  //painter->drawRect(boundingRect());
  painter->setPen(currentText);
  painter->drawText(
      boundingRect(), QString{"%1\n%2"}.arg(noteText(m_value)).arg(m_value),
      QTextOption(Qt::AlignLeft));
}
}

W_OBJECT_IMPL(score::QGraphicsNoteChooser)
