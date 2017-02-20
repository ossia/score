#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <QPainter>
#include <qnamespace.h>

#include "DummyLayerView.hpp"
#include <Process/LayerView.hpp>

#include <iscore/model/Skin.hpp>
/*
#include <QGraphicsLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QQuickWidget>
#include <QOpenGLPaintDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QVBoxLayout>
#include <QWidget>

#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
*/
namespace Dummy
{
// TODO MOVEME in iscore/widgets
class DummyTextItem final : public QQuickPaintedItem
{
public:
  DummyTextItem(QQuickPaintedItem* parent) : QQuickPaintedItem{parent}
  {
  }

  QRectF boundingRect() const override
  {
    QRect rect;
    return QFontMetrics(m_font).boundingRect(rect, Qt::AlignCenter, m_text);
  }

  void setFont(const QFont& font)
  {
//    prepareGeometryChange();
    m_font = font;
    m_font.setPointSizeF(30);
    update();
  }

  void setText(const QString& text)
  {
    m_text = text;
    update();
  }

  void paint(
      QPainter* painter) override
  {
    painter->setFont(m_font);
    painter->setPen(Qt::lightGray);
    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
  }

private:
  QString m_text;
  QFont m_font;
};
DummyLayerView::DummyLayerView(QQuickPaintedItem* parent)
    : LayerView{parent}, m_text{new DummyTextItem{this}}
{
  m_text->setFont(iscore::Skin::instance().SansFont);
  connect(
      this, &DummyLayerView::heightChanged, this, &DummyLayerView::updateText);
  connect(
      this, &DummyLayerView::widthChanged, this, &DummyLayerView::updateText);
  /*
  m_view = new QQuickWidget();
  m_view->setSource(QUrl("qrc:/DummyProcess.qml"));

  m_view->create();

  m_item = m_view->rootObject()->findChild<QQuickItem*>("input");
  connect(m_view, &QQuickWidget::sceneGraphInvalidated,
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

void DummyLayerView::updateText()
{
  auto w = width();
  auto h = height();

  auto trect = m_text->boundingRect();
  auto tw = trect.width();
  auto th = trect.height();

  auto fw = w / tw;
  auto fh = h / th;
  if (fw >= 1. && fh >= 1.)
  {
    m_text->setScale(1.);
  }
  else
  {
    auto min = std::min(fw, fh);
    m_text->setScale(min);
  }

  m_text->setPosition(QPointF(w / 2., h / 2.));
}

void DummyLayerView::paint_impl(QPainter* painter) const
{
  /*
  painter->drawImage(0, 0, m_view->grabWindow());
  */
}

void DummyLayerView::mousePressEvent(QMouseEvent* ev)
{
  /*
  auto nev = new QMouseEvent(QEvent::Type::MouseButtonPress, ev->pos(),
  ev->button(), ev->buttons(), ev->modifiers());
  m_view->sendEvent(m_item, nev);
  m_view->requestUpdate();
  */
  emit pressed();
}
void DummyLayerView::mouseMoveEvent(QMouseEvent* ev)
{
  /*
  auto nev = new QMouseEvent(QEvent::Type::MouseMove, ev->pos(), ev->button(),
  ev->buttons(), ev->modifiers());
  m_view->sendEvent(m_item, nev);
  m_view->requestUpdate();
  */
}
void DummyLayerView::mouseReleaseEvent(QMouseEvent* ev)
{
  /*
  auto nev = new QMouseEvent(QEvent::Type::MouseButtonRelease, ev->pos(),
  ev->button(), ev->buttons(), ev->modifiers());
  m_view->sendEvent(m_item, nev);
  m_view->requestUpdate();
  */
}
}
