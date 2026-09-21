#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/widgets/QGraphicsMultiSliderXY.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <limits>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsMultiSliderXY);

namespace score
{
score::QGraphicsMultiSliderXY::QGraphicsMultiSliderXY(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void score::QGraphicsMultiSliderXY::setPoint(const QPointF& r)
{
  SCORE_TODO;
}

static constexpr double cursorPickMargin = 4.;

QRectF
score::QGraphicsMultiSliderXY::cursorRect(const ossia::vec2f& cursor) const noexcept
{
  return QRectF{
      (cursor[0] - cursorSize.x / 2) * width(),
      (1 - cursor[1] - cursorSize.y / 2) * height(), cursorSize.x * width(),
      cursorSize.y * height()};
}

QRectF
score::QGraphicsMultiSliderXY::cursorPickRect(const ossia::vec2f& cursor) const noexcept
{
  return cursorRect(cursor).adjusted(
      -cursorPickMargin, -cursorPickMargin, cursorPickMargin, cursorPickMargin);
}

int score::QGraphicsMultiSliderXY::cursorAt(QPointF p) const noexcept
{
  int found = -1;
  double closest = std::numeric_limits<double>::max();
  for (int v = 0; v < std::ssize(tab); v++)
  {
    const auto rect = cursorPickRect(tab[v]);
    if (!rect.contains(p))
      continue;

    const auto d = rect.center() - p;
    const double dist = d.x() * d.x() + d.y() * d.y();
    if (dist < closest)
    {
      closest = dist;
      found = v;
    }
  }
  return found;
}

void score::QGraphicsMultiSliderXY::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();

  // Draw the background
  painter->fillRect(boundingRect(), skin.Dark);

  int i = 0;
  const auto& font = skin.MonoFontSmall;
  painter->setFont(font);
  painter->setBrush(skin.Base4.darker.brush);

  static const QTextOption p = QTextOption{Qt::AlignCenter};
  for (const ossia::vec2f& cursor : tab)
  {
      const auto rect = cursorRect(cursor);
      painter->setPen(skin.Base4.main.pen0);
      painter->drawRect(rect);


      painter->setPen(skin.Light.lighter180.pen0);
      painter->drawText(rect, QString::number(i++), p);
  }
}

ossia::value score::QGraphicsMultiSliderXY::value() const
{
  return m_value;
}

void score::QGraphicsMultiSliderXY::setRange(const ossia::value& min, const ossia::value& max)
{
  this->min = ossia::convert<float>(min);
  this->max = ossia::convert<float>(max);
}

void score::QGraphicsMultiSliderXY::setRange(const ossia::domain& dom)
{
  auto [min, max] = ossia::get_float_minmax(dom);

  if (min)
    this->min = *min;
  if (max)
    this->max = *max;
}

void score::QGraphicsMultiSliderXY::setValue(ossia::value v)
{
  prepareGeometryChange();
  m_value = ossia::convert<std::vector<ossia::value>>(std::move(v));
  tab.clear();
  for(auto& vec : m_value) {
    tab.push_back(ossia::convert<ossia::vec2f>(vec));
  }
  update();
}

void score::QGraphicsMultiSliderXY::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab && ossia::valid_index(selectedCursor, tab))  // The cursor’s position is updated as the mouse moves, constrained within the widget’s bounds.
  {
    tab[selectedCursor] = ossia::vec2f{(float)std::clamp((event->pos().x() / width()), 0., 1.),
                                       (float)std::clamp(1-(event->pos().y() / height()), 0., 1.)};
    m_value = std::vector<ossia::value>(tab.begin(), tab.end());
    sliderMoved();
    update();
  }
}

void score::QGraphicsMultiSliderXY::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto pos = event->pos();

  // If the left mouse button is pressed on an existing cursor, that cursor can be moved.
  if (event->button() & Qt::LeftButton)
  {
    if (const int v = cursorAt(pos); v >= 0)
    {
      m_grab = true;
      selectedCursor = v;
      mouseMoveEvent(event);
      return;
    }

    // Else if the press occurs at an empty area, a new cursor will be created at that position.
    tab.push_back(
        ossia::vec2f{(float)(pos.x() / width()), 1 - (float)(pos.y() / height())});
    m_value = std::vector<ossia::value>(tab.begin(), tab.end());
    m_grab = true;
    selectedCursor = std::ssize(tab) - 1;
    mouseMoveEvent(event);
    return;
  }
  else if (event->button() & Qt::RightButton) //If the right mouse button is pressed over a cursor, the cursor is deleted.
  {
    m_grab = false;
    if (const int v = cursorAt(pos); v >= 0)
    {
      tab.erase(tab.begin() + v);
      selectedCursor = -1;
      m_value = std::vector<ossia::value>(tab.begin(), tab.end());
      sliderMoved();
    }
    return;
  }
  update();
}

void score::QGraphicsMultiSliderXY::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if ((event->button() & Qt::LeftButton) && m_grab)
  {
    mouseMoveEvent(event);
    m_grab = false;
    selectedCursor=-1;
  }

  update();
  sliderReleased();
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on. See DefaultGraphicsSliderImpl for what goes
//! wrong if the edit is left open.
bool score::QGraphicsMultiSliderXY::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(m_grab)
    {
      m_grab = false;
      selectedCursor = -1;
      update();
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

QRectF score::QGraphicsMultiSliderXY::boundingRect() const
{
  return QRectF(0, 0, 400, 400);
}
}
