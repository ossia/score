#pragma once
#include <score/graphics/InfiniteScroller.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Cursor.hpp>
#include <score/widgets/DoubleSpinBox.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/math.hpp>

#include <QDoubleSpinBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>

namespace score
{
struct DefaultGraphicsKnobImpl
{
  template <typename T>
  static void paint(
      T& self, const score::Skin& skin, const QString& text, QPainter* painter,
      QWidget* widget)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);

    constexpr const double adj = 6.;
    constexpr const double space = 50.;
    constexpr const double start = (270. - space) * 16.;
    constexpr const double totalSpan = (360. - 2. * space) * 16.;

    const QRectF srect = self.boundingRect();
    const QRectF r = srect.adjusted(adj, adj, -adj, -adj);
    const double rw = r.width();

    // Draw knob
    painter->setPen(skin.Emphasis2.main.pen1);
    painter->setBrush(skin.Emphasis2.main.brush);
    painter->drawChord(r, start, -totalSpan);

    const double valueSpan = -self.m_value * totalSpan;
    double textDelta = 0.;
    if(rw >= 30.)
    {
      painter->setPen(skin.Base4.main.pen3_solid_round_round);
      textDelta = -10;
    }
    else if(rw >= 20.)
    {
      painter->setPen(skin.Base4.main.pen2_solid_round_round);
      textDelta = -9;
    }
    else if(rw >= 10.)
    {
      painter->setPen(skin.Base4.main.pen1_5);
      textDelta = -8;
    }
    else if(rw >= 5.)
    {
      painter->setPen(skin.Base4.main.pen1);
      textDelta = -7;
    }
    painter->drawArc(r, start, valueSpan);

    // Draw knob indicator
    const double r1 = 0.5 * rw;
    const double x0 = r.center().x();
    const double y0 = r.center().y();
    const double theta = -0.0174533 * (start + valueSpan) / 16.;
    const double x1 = r.center().x() + r1 * cos(theta);
    const double y1 = r.center().y() + r1 * sin(theta);

    painter->drawLine(QPointF{x0, y0}, QPointF{x1, y1});

    painter->setPen(skin.Base4.lighter180.pen1);
    if(self.m_hasExec)
    {
      const QRectF er = r.adjusted(1.5, 1.5, -1.5, -1.5);
      const double valueSpan = -self.m_execValue * totalSpan;
      painter->drawArc(er, start, valueSpan);
    }

    // Draw text
    painter->setFont(skin.Medium8Pt);
    painter->drawText(
        QRectF{0., srect.height() + textDelta, srect.width(), 10.}, text,
        QTextOption(Qt::AlignCenter));

    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  template <typename T>
  static void mousePressEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton)
    {
      self.m_grab = true;
      InfiniteScroller::start(self, self.m_value);
    }

    event->accept();
  }

  template <typename T>
  static void mouseMoveEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      double v = InfiniteScroller::move(event);
      if(v != self.m_value)
      {
        self.m_value = v;
        self.sliderMoved();
        self.update();
      }
    }
    event->accept();
  }

  template <typename T>
  static void mouseReleaseEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    InfiniteScroller::stop(self, event);
    if(self.m_grab)
    {
      double v = InfiniteScroller::move(event);
      if(v != self.m_value)
      {
        self.m_value = v;
        self.update();
      }
    }

    self.m_grab = false;
    self.sliderReleased();

    if(event->button() == Qt::RightButton)
    {
      contextMenuEvent(self, event->scenePos());
    }

    event->accept();
  }

  template <typename T>
  static void contextMenuEvent(T& self, QPointF pos)
  {
    QTimer::singleShot(0, [&, self_p = &self, pos] {
      auto w = new DoubleSpinboxWithEnter;
      w->setRange(self.min, self.max);

      w->setDecimals(6);
      w->setValue(self.map(self.m_value));
      auto obj = self.scene()->addWidget(
          w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
      obj->setPos(pos);

      QTimer::singleShot(0, w, [w] { w->setFocus(); });

      auto con = QObject::connect(
          w, SignalUtils::QDoubleSpinBox_valueChanged_double(), &self,
          [&self](double v) {
        self.m_value = self.unmap(v);
        self.sliderMoved();
        self.update();
          });

      QObject::connect(
          w, &DoubleSpinboxWithEnter::editingFinished, &self,
          [obj, con, self_p]() mutable {
        if(obj != nullptr)
        {
          self_p->sliderReleased();
          QObject::disconnect(con);
          QTimer::singleShot(0, obj, [scene = self_p->scene(), obj] {
            scene->removeItem(obj);
            delete obj;
          });
        }
        obj = nullptr;
          });
    });
  }

  template <typename T>
  static void mouseDoubleClickEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
  }
};
}
