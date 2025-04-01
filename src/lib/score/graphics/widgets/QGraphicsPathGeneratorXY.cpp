#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/widgets/QGraphicsPathGeneratorXY.hpp>
#include <score/serialization/StringConstants.hpp>
#include <score/tools/Debug.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <utility>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPathGeneratorXY);

namespace score
{
score::QGraphicsPathGeneratorXY::QGraphicsPathGeneratorXY(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void score::QGraphicsPathGeneratorXY::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  tab = m_value.get<std::vector<ossia::value>>();
  auto& skin = score::Skin::instance();

  // Draw the background
  painter->fillRect(QRectF(0, 0, width(), height()), QColor(skin.Dark.color()));

  // Draw cursors
    for (size_t s = 0; s < std::ssize(tab); s++){
      for (size_t c = 0; c < std::ssize(tab[s].get<std::vector<ossia::value>>()); c++)
      {

    ossia::vec2f& cursorXY = tab[s].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>();

    painter->fillRect(
        QRectF((cursorXY[0] - cursorSize.x / 2) * width(),
               (1-cursorXY[1] - cursorSize.y / 2) * height(),
               cursorSize.x * width(),
               cursorSize.y * height()),
        s == selectedSource ? QColor(c == 0 ? skin.Base3.color(): skin.Warn3.color()):QColor(skin.DarkGray.color()));

    }
      if(s==selectedSource){
        auto& point = tab[s].get<std::vector<ossia::value>>();
        painter->drawLine(
            QPointF((point[0].get<ossia::vec2f>()[0]*width()),
                    (1-point[0].get<ossia::vec2f>()[1])*height()),
            QPointF((point[1].get<ossia::vec2f>()[0]*width()),
                    (1-point[1].get<ossia::vec2f>()[1])*height()));

      }
}
}

ossia::value score::QGraphicsPathGeneratorXY::value() const
{
  return m_value;
}

void score::QGraphicsPathGeneratorXY::setRange(const ossia::value& min, const ossia::value& max)
{
  this->min = ossia::convert<float>(min);
  this->max = ossia::convert<float>(max);
}

void score::QGraphicsPathGeneratorXY::setRange(const ossia::domain& dom)
{
  auto [min, max] = ossia::get_float_minmax(dom);

  if (min)
    this->min = *min;
  if (max)
    this->max = *max;
}

void score::QGraphicsPathGeneratorXY::setValue(ossia::value v)
{
  prepareGeometryChange();
  m_value = std::move(v);
  update();
}

void score::QGraphicsPathGeneratorXY::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab && ossia::valid_index(selectedSource, tab)) // The cursor’s position is updated as the mouse moves, constrained within the widget’s bounds.
  {
    tab[selectedSource].get<std::vector<ossia::value>>()[selectedCursor] =
                                  ossia::vec2f{
                                       (float)std::clamp((event->pos().x() / width()), 0., 1.),
                                       (float)std::clamp(1-(event->pos().y() / height()), 0., 1.)};

    m_value = std::vector<ossia::value>(tab.begin(), tab.end());
    sliderMoved();
    update();
  }
}

void score::QGraphicsPathGeneratorXY::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  float x = event->pos().x();
  float y = event->pos().y();

  // If the left mouse button is pressed on an existing cursor, that cursor can be moved.
  if (event->button() & Qt::LeftButton)
  {
    for (int v = 0; v < std::ssize(tab); v++){
      for (int c = 0; c < std::ssize(tab[v].get<std::vector<ossia::value>>()); c++)
    {
      if ((tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[0] - 0.02f) * width() <= (float)x &&
         (float)x <= (tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[0] + 0.02f) * width() &&
         (1-tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[1] - 0.02f) * height() <= (float)y &&
         (float)y <= (1-tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[1] + 0.02f) * height())
      {
        m_grab = true;
        selectedSource = v;
        selectedCursor = c;
        mouseMoveEvent(event);
        return;
      }
    }
    }
    // Else if the press occurs at an empty area, a new cursor will be created at that position.
    tab.push_back(std::vector<ossia::value>{ossia::vec2f{(float)(x / width()), 1-(float)(y / width())}, ossia::vec2f{(float)(x / width()), 1-(float)(y / width())}});

    m_value = std::vector<ossia::value>(tab.begin(), tab.end());
    m_grab = true;
    selectedSource = tab.size() - 1;
    selectedCursor = 1;
    mouseMoveEvent(event);
    return;
  }
  else if (event->button() & Qt::RightButton) //If the right mouse button is pressed over a cursor, the cursor is deleted.
  {
    m_grab = false;
    for (int v = 0; v < std::ssize(tab); v++){
      for (int c = 0; c < std::ssize(tab[v].get<std::vector<ossia::value>>()); c++)
    {
      if ((tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[0] - 0.02f) * width() <= (float)x &&
         (float)x <= (tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[0] + 0.02f) * width() &&
         (1-tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[1] - 0.02f) * height() <= (float)y &&
         (float)y <= (1-tab[v].get<std::vector<ossia::value>>()[c].get<ossia::vec2f>()[1] + 0.02f) * height())
      {
        tab.erase(tab.begin() + v);
        selectedSource--;
        m_value = std::vector<ossia::value>(tab.begin(), tab.end());
        sliderMoved();
        return;
      }}

    }
    return;
  }
  update();
}

void score::QGraphicsPathGeneratorXY::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if ((event->button() & Qt::LeftButton) && m_grab)
  {
    m_grab = false;
    mouseMoveEvent(event);
  }

  update();
  sliderReleased();
}

QRectF score::QGraphicsPathGeneratorXY::boundingRect() const
{
  return QRectF(0, 0, 400, 400);
}
}
