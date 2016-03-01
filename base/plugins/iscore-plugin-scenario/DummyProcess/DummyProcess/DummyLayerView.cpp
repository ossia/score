#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <qnamespace.h>
#include <QPainter>

#include "DummyLayerView.hpp"
#include <Process/LayerView.hpp>

#include <Process/Style/Skin.hpp>

#include <QQuickView>
#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QGraphicsLayout>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>

namespace Dummy
{
DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{
    QQuickView *view = new QQuickView();
    view->setSource(QUrl("qrc:/DummyProcess.qml"));
    QWidget *container = QWidget::createWindowContainer(view, this->scene()->views().front());
    container->setMinimumSize(QSize{100, 100});
    container->setMaximumSize(QSize{500, 500});
    container->setFocusPolicy(Qt::TabFocus);
    QGraphicsProxyWidget * item = new QGraphicsProxyWidget(this);
    item->setWidget(container);
    view->show();

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
    auto f = Skin::instance().SansFont;
    f.setPointSize(30);
    painter->setFont(f);
    painter->setPen(Qt::lightGray);

    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}

void DummyLayerView::mousePressEvent(QGraphicsSceneMouseEvent*)
{
    emit pressed();
}
}
