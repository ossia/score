#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <qnamespace.h>
#include <QPainter>

#include "DummyLayerView.hpp"
#include <Process/LayerView.hpp>

#include <Process/Style/Skin.hpp>
/*
#include <QQuickWindow>
#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QGraphicsLayout>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QQuickView>
#include <QOpenGLPaintDevice>
#include <QQuickItem>
#include <QQuickRenderControl>

#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
*/
namespace Dummy
{
DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{
    /*
    m_view = new QQuickView();
    m_view->setSource(QUrl("qrc:/DummyProcess.qml"));

    m_view->create();

    m_item = m_view->rootObject()->findChild<QQuickItem*>("input");
    connect(m_view, &QQuickView::sceneGraphInvalidated,
            this, [=] { update(); });

    connect(this, &DummyLayerView::heightChanged,
            this, [=] {
        m_view->setHeight(height() - 5);
    });

    connect(this, &DummyLayerView::widthChanged,
            this, [=] {
        m_view->setWidth(width());
    });
    */
}

void DummyLayerView::paint_impl(QPainter* painter) const
{
    /*
    painter->drawImage(0, 0, m_view->grabWindow());
    */

    auto f = Skin::instance().SansFont;
    f.setPointSize(30);
    painter->setFont(f);
    painter->setPen(Qt::lightGray);

    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}

void DummyLayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    /*
    auto nev = new QMouseEvent(QEvent::Type::MouseButtonPress, ev->pos(), ev->button(), ev->buttons(), ev->modifiers());
    m_view->sendEvent(m_item, nev);
    m_view->requestUpdate();
    */
    emit pressed();
}
void DummyLayerView::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    /*
    auto nev = new QMouseEvent(QEvent::Type::MouseMove, ev->pos(), ev->button(), ev->buttons(), ev->modifiers());
    m_view->sendEvent(m_item, nev);
    m_view->requestUpdate();
    */
}
void DummyLayerView::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    /*
    auto nev = new QMouseEvent(QEvent::Type::MouseButtonRelease, ev->pos(), ev->button(), ev->buttons(), ev->modifiers());
    m_view->sendEvent(m_item, nev);
    m_view->requestUpdate();
    */
}
}
