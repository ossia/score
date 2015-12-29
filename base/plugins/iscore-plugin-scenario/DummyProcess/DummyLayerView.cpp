#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <qnamespace.h>
#include <QPainter>

#include "DummyLayerView.hpp"
#include <Process/LayerView.hpp>

/*
#include <QQuickWidget>
#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QGraphicsLayout>
#include <QQuickItem>
class QGraphicsItem;
#include <QPushButton>
*/
DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{
    /*
    auto obj = new QGraphicsProxyWidget(this);

    m_widg = new QQuickWidget;
    m_widg->setSource(QUrl("qrc:/DummyProcess.qml"));
    m_widg->setFormat(QSurfaceFormat::defaultFormat());

    obj->setWidget(m_widg);
    m_widg->show();
    connect(this, &QGraphicsObject::widthChanged,
            this, [=] () { m_widg->rootObject()->setWidth(this->width());});
    connect(this, &QGraphicsObject::heightChanged,
            this, [=] () { m_widg->rootObject()->setHeight(this->height() - 5);});
            */
}

void DummyLayerView::paint_impl(QPainter* painter) const
{
    auto f = ProcessFonts::Sans();
    f.setPointSize(30);
    painter->setFont(f);
    painter->setPen(Qt::lightGray);

    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}
