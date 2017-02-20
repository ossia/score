#include <Process/Style/ProcessFonts.hpp>
#include <QFlags>
#include <QFont>
#include <QQuickPaintedItem>
#include <QPainter>
#include <QRect>
#include <qnamespace.h>

#include "MappingView.hpp"
#include <Process/LayerView.hpp>
#include <iscore/model/Skin.hpp>

static const int fontSize = 8;
namespace Mapping
{
LayerView::LayerView(QQuickPaintedItem* parent) : Process::LayerView{parent}
{
  setZ(1);
  this->setFlags(
      ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);

  auto f = iscore::Skin::instance().SansFont;
  f.setPointSize(fontSize);

  m_textcache.setFont(f);
  m_textcache.setCacheEnabled(true);
}

void LayerView::showName(bool b)
{
  m_showName = b;

  update();
}

void LayerView::setDisplayedName(const QString& s)
{
  m_displayedName = s;

  // TODO refactor with automation
  m_textcache.setText(s);
  m_textcache.beginLayout();
  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

  update();
}

void LayerView::paint_impl(QPainter* painter) const
{
  if (m_showName)
  {
    m_textcache.draw(painter, QPointF{5., double(fontSize)});
  }
}
}
