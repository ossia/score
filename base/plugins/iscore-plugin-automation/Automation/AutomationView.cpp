#include <Process/Style/ProcessFonts.hpp>
#include <QFlags>
#include <QFont>
#include <QQuickPaintedItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QRect>
#include <iscore/model/Skin.hpp>
#include <qnamespace.h>

#include "AutomationView.hpp"
#include <Process/LayerView.hpp>

const int fontSize = 8;
namespace Automation
{
LayerView::LayerView(QQuickPaintedItem* parent) : Process::LayerView{parent}
{
  setZ(1);
  this->setFlag(QQuickPaintedItem::ItemClipsChildrenToShape, true);
  //setFlags(ItemClipsChildrenToShape /* | ItemIsSelectable | ItemIsFocusable */);
  //setAcceptDrops(true);
  auto f = iscore::Skin::instance().SansFont;
  f.setPointSize(fontSize);

  m_textcache.setFont(f);
  m_textcache.setCacheEnabled(true);
}

LayerView::~LayerView()
{
}

void LayerView::setDisplayedName(const QString& s)
{
  m_textcache.setText(s);
  m_textcache.beginLayout();
  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

  update();
}

void LayerView::paint_impl(QPainter* painter) const
{
#if !defined(ISCORE_IEEE_SKIN)
  if (m_showName)
  {
    m_textcache.draw(painter, QPointF{5., double(fontSize)});
  }
#endif
}

void LayerView::dropEvent(QDropEvent* event)
{
  if (event->mimeData())
    emit dropReceived(*event->mimeData());
}
}
