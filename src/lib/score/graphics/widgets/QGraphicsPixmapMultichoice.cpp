#include <score/graphics/widgets/QGraphicsPixmapMultichoice.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPixmapMultichoice);

namespace score
{
QGraphicsPixmapMultichoice::QGraphicsPixmapMultichoice(QGraphicsItem* parent)
    : QGraphicsPixmapItem{parent}
{
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsPixmapMultichoice::toggle()
{
  SCORE_ASSERT(!m_states.empty());
  setState((m_current + 1) % m_states.size());
}

void QGraphicsPixmapMultichoice::setState(int toggled)
{
  if(toggled != m_current)
  {
    m_current = toggled;
    if(ossia::valid_index(m_current, m_states))
      setPixmap(m_states[m_current]);
  }
}

void QGraphicsPixmapMultichoice::setPixmaps(std::vector<QPixmap> states)
{
  m_states = std::move(states);
  SCORE_ASSERT(!m_states.empty());
  m_current = m_current % m_states.size();
  setPixmap(m_states[m_current]);
}

void QGraphicsPixmapMultichoice::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  toggle();
  toggled(m_current);
  event->accept();
}

void QGraphicsPixmapMultichoice::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsPixmapMultichoice::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
}
