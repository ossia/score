#include <Process/Style/ProcessFonts.hpp>
#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QRect>

#include "MappingView.hpp"
#include <Process/LayerView.hpp>
#include <iscore/model/Skin.hpp>

static const int fontSize = 8;
namespace Mapping
{
MappingView::MappingView(QGraphicsItem* parent) :
    LayerView {parent}
{
    setZValue(1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);

    auto f = iscore::Skin::instance().SansFont;
    f.setPointSize(fontSize);

    m_textcache.setFont(f);
    m_textcache.setCacheEnabled(true);
}

void MappingView::showName(bool b)
{
    m_showName = b;

    update();
}

void MappingView::setDisplayedName(const QString& s)
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

void MappingView::paint_impl(QPainter* painter) const
{
    if(m_showName)
    {
        m_textcache.draw(painter, QPointF{5., double(fontSize)});
    }
}
}
