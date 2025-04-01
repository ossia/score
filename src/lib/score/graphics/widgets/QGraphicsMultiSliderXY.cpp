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

void score::QGraphicsMultiSliderXY::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{

  tab = m_value.get<std::vector<ossia::value>>();

  auto& skin = score::Skin::instance();

  // Draw the background
  painter->fillRect(boundingRect(), skin.Dark);

  // Draw cursors
  for (const ossia::value& v : tab)
  {
    auto& cursor = v.get<ossia::vec2f>();
    painter->fillRect(
        QRectF((cursor[0] - cursorSize.x / 2) * width(),
               (1-cursor[1] - cursorSize.y / 2) * height(),
               cursorSize.x * width(),
               cursorSize.y * height()),
        skin.Base2);
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
  m_value = std::move(v);
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
  float x = event->pos().x();
  float y = event->pos().y();

  // If the left mouse button is pressed on an existing cursor, that cursor can be moved.
  if (event->button() & Qt::LeftButton)
  {
    for (int v = 0; v < std::ssize(tab) ; v++)
    {
      auto& point = tab[v].get<ossia::vec2f>();
      if ((point[0] - 0.02f) * width() <= (float)x &&
         (float)x <= (point[0] + 0.02f) * width() &&
         (1-point[1] - 0.02f) * height() <= (float)y &&
         (float)y <= (1-point[1] + 0.02f) * height())
      {
        m_grab = true;
        selectedCursor = v;
        mouseMoveEvent(event);
        return;
      }
    }
    // Else if the press occurs at an empty area, a new cursor will be created at that position.
    tab.push_back(ossia::vec2f{(float)(x / width()), 1-(float)(y / width())});
    m_value = std::vector<ossia::value>(tab.begin(), tab.end());
    m_grab = true;
    selectedCursor = std::ssize(tab) - 1;
    mouseMoveEvent(event);
    return;
  }
  else if (event->button() & Qt::RightButton) //If the right mouse button is pressed over a cursor, the cursor is deleted.
  {
    m_grab = false;
    for (int v = 0; v < std::ssize(tab); v++)
    {
      auto& point = tab[v].get<ossia::vec2f>();
      if ((point[0] - 0.02f) * width() <= (float)x &&
         (float)x <= (point[0] + 0.02f) * width() &&
         (1-point[1] - 0.02f) * height() <= (float)y &&
         (float)y <= (1-point[1] + 0.02f) * height())
      {
        tab.erase(tab.begin() + v);
        selectedCursor=-1;
        m_value = std::vector<ossia::value>(tab.begin(), tab.end());
        sliderMoved();
        return;
      }
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

QRectF score::QGraphicsMultiSliderXY::boundingRect() const
{
  return QRectF(0, 0, 400, 400);
}
}
