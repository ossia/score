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
class TextItem : public QGraphicsItem
{
    public:
        TextItem(QGraphicsItem* parent):
            QGraphicsItem{parent}
        {

        }

        QRectF boundingRect() const override
        {
            QRect rect;
            return QFontMetrics(m_font).boundingRect(rect, Qt::AlignCenter, m_text);
        }

        void setFont(const QFont& font)
        {
            prepareGeometryChange();
            m_font = font;
            m_font.setPointSizeF(30);
            update();
        }

        void setText(const QString& text)
        {
            m_text = text;
            update();
        }

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
        {
            painter->setFont(m_font);
            painter->setPen(Qt::lightGray);
            painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
        }

    private:
        QString m_text;
        QFont m_font;
};

DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent},
    m_text{new TextItem{this}}
{
    m_text->setFont(Skin::instance().SansFont);
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

void DummyLayerView::setText(const QString& text)
{
    m_text->setText(text);
    update();
}

void DummyLayerView::paint_impl(QPainter* painter) const
{
    auto w = width();
    auto h = height();

    auto trect = m_text->boundingRect();
    auto tw = trect.width();
    auto th = trect.height();

    auto fw = w / tw;
    auto fh = h / th;
    if(fw >= 1. && fh >= 1.)
    {
        m_text->setScale(1.);
    }
    else
    {
        auto min = std::min(fw, fh);
        m_text->setScale(min);
    }

    m_text->setPos(w / 2., h / 2.);

    /*
    painter->drawImage(0, 0, m_view->grabWindow());
    */
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
