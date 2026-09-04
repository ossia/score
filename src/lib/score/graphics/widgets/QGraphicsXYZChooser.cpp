#include <score/graphics/TypeInWidget.hpp>
#include <score/graphics/widgets/QGraphicsXYZChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsXYZChooser);

namespace score
{

QGraphicsXYZChooser::QGraphicsXYZChooser(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setRange();
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

QGraphicsXYZChooser::~QGraphicsXYZChooser()
{
  if(m_grab)
    sliderReleased();
}

void QGraphicsXYZChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->fillRect(QRectF{0, 0, 100, 100}, score::Skin::instance().Dark.main.brush);
  painter->fillRect(QRectF{110, 0, 20, 100}, score::Skin::instance().Dark.main.brush);

  auto [x, y, z] = m_value;
  x = 100. * (x - m_min[0]) / (m_max[0] - m_min[0]);
  y = 100. * (1. - ((y - m_min[1]) / (m_max[1] - m_min[1])));
  z = 100. * (1. - ((z - m_min[2]) / (m_max[2] - m_min[2])));

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0., y}, QPointF{100., y});
  painter->drawLine(QPointF{110., z}, QPointF{130., z});
}

std::array<float, 3> QGraphicsXYZChooser::value() const
{
  return m_value;
}

ossia::vec3f QGraphicsXYZChooser::scaledValue(float x, float y, float z) const noexcept
{
  return {
      m_min[0] + x * (m_max[0] - m_min[0]), m_min[1] + y * (m_max[1] - m_min[1]),
      m_min[2] + z * (m_max[2] - m_min[2])};
}

//! The drag state, which the z strip carries across presses: a press in the
//! xy square reads x and y off the click and keeps whatever z it had, so a
//! value that came from elsewhere has to land here too or the next drag puts
//! the old z back.
void QGraphicsXYZChooser::rescale() noexcept
{
  for(int i = 0; i < 3; i++)
    prev_v[i]
        = (m_max[i] > m_min[i]) ? (m_value[i] - m_min[i]) / (m_max[i] - m_min[i]) : 0.f;
}

void QGraphicsXYZChooser::setValue(ossia::vec3f v)
{
  m_value = v;
  rescale();
  update();
}

void QGraphicsXYZChooser::setRange(ossia::vec3f min, ossia::vec3f max, ossia::vec3f init)
{
  m_min = min;
  m_max = max;
  m_init = init;
  rescale();
}

void QGraphicsXYZChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  // Left button only: the right one raises the type-in boxes on release, and
  // must not move the point on the way there.
  if(event->button() != Qt::LeftButton)
  {
    event->accept();
    return;
  }

  const auto p = event->pos();
  if(p.x() < 100.)
  {
    prev_v[0] = qBound(0., p.x() / 100., 1.);
    prev_v[1] = qBound(0., 1 - (p.y() / 100.), 1.);
  }
  else if(p.x() >= 110 && p.x() < 130)
  {
    prev_v[2] = qBound(0., 1 - (p.y() / 100.), 1.);
  }
  m_grab = true;

  const ossia::vec3f newValue = scaledValue(prev_v[0], prev_v[1], prev_v[2]);
  if(m_value != newValue)
  {
    m_value = newValue;
    sliderMoved();
    update();
  }
  event->accept();
}

void QGraphicsXYZChooser::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  // FIXME
}

void QGraphicsXYZChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto p = event->pos();
    if(p.x() < 100.)
    {
      prev_v[0] = qBound(0., p.x() / 100., 1.);
      prev_v[1] = qBound(0., 1 - (p.y() / 100.), 1.);
    }
    else if(p.x() >= 110 && p.x() <= 130)
    {
      prev_v[2] = qBound(0., 1 - (p.y() / 100.), 1.);
    }
    m_grab = true;

    const ossia::vec3f newValue = scaledValue(prev_v[0], prev_v[1], prev_v[2]);
    if(m_value != newValue)
    {
      m_value = newValue;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsXYZChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto p = event->pos();
    if(p.x() < 100.)
    {
      prev_v[0] = qBound(0., p.x() / 100., 1.);
      prev_v[1] = qBound(0., 1 - (p.y() / 100.), 1.);
    }
    else if(p.x() >= 110 && p.x() < 130)
    {
      prev_v[2] = qBound(0., 1 - (p.y() / 100.), 1.);
    }

    const ossia::vec3f newValue = scaledValue(prev_v[0], prev_v[1], prev_v[2]);
    if(m_value != newValue)
    {
      m_value = newValue;
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  else if(event->button() == Qt::RightButton)
  {
    showTypeIn(event->scenePos());
  }
  event->accept();
}

void QGraphicsXYZChooser::showTypeIn(QPointF scenePos)
{
  auto* sc = scene();
  if(!sc)
    return;

  showTypeInBox(
      *sc, scenePos,
      {TypeInField{QStringLiteral("x"), m_min[0], m_max[0], m_value[0]},
       TypeInField{QStringLiteral("y"), m_min[1], m_max[1], m_value[1]},
       TypeInField{QStringLiteral("z"), m_min[2], m_max[2], m_value[2]}},
      [this](int i, double v) {
    m_value[i] = v;
    rescale();
    sliderMoved();
    update();
  }, [this] { sliderReleased(); });
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on. See DefaultGraphicsSliderImpl for what goes
//! wrong if the edit is left open.
bool QGraphicsXYZChooser::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(m_grab)
    {
      m_grab = false;
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

QRectF QGraphicsXYZChooser::boundingRect() const
{
  return QRectF{0, 0, 140, 100};
}

}
