#include "InterpolationView.hpp"
#include <Process/Style/ProcessFonts.hpp>
#include <QGraphicsSceneMouseEvent>
#include <iscore/model/Skin.hpp>

const int fontSize = 8;
namespace Interpolation
{
View::View(QQuickPaintedItem* parent) : Process::LayerView{parent}
{
  setZ(1);
  setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
  setAcceptDrops(true);
  auto f = iscore::Skin::instance().SansFont;
  f.setPointSize(fontSize);

  m_textcache.setFont(f);
  m_textcache.setCacheEnabled(true);
}

View::~View()
{
}

void View::setDisplayedName(const QString& s)
{
  m_textcache.setText(s);
  m_textcache.beginLayout();
  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

  update();
}

void View::paint_impl(QPainter* painter) const
{
#if !defined(ISCORE_IEEE_SKIN)
  if (m_showName)
  {
    m_textcache.draw(painter, QPointF{5., double(fontSize)});
  }
#endif
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->mimeData());
}
}
