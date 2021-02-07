#pragma once
#include <score/model/Skin.hpp>
#include <score/tools/Cursor.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/DoubleSpinBox.hpp>

#include <ossia/detail/math.hpp>

#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>

namespace score
{
struct DefaultGraphicsKnobImpl
{
  static inline double origValue{};
  static inline double currentDelta{};
  static inline QRectF currentGeometry{};

  template <typename T>
  static void
  paint(T& self, const score::Skin& skin, const QString& text, QPainter* painter, QWidget* widget)
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
    if (rw >= 30.)
    {
      painter->setPen(skin.Base4.main.pen3_solid_round_round);
      textDelta = -10;
    }
    else if (rw >= 20.)
    {
      painter->setPen(skin.Base4.main.pen2_solid_round_round);
      textDelta = -9;
    }
    else if (rw >= 10.)
    {
      painter->setPen(skin.Base4.main.pen1_5);
      textDelta = -8;
    }
    else if (rw >= 5.)
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

    // Draw text
    painter->setPen(skin.Base4.lighter180.pen1);
    painter->setFont(skin.Medium8Pt);
    painter->drawText(
        QRectF{0., srect.height() + textDelta, srect.width(), 10.},
        text,
        QTextOption(Qt::AlignCenter));

    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  template <typename T>
  static void mousePressEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if (event->button() == Qt::LeftButton)
    {
      self.m_grab = true;
      self.setCursor(QCursor(Qt::BlankCursor));
      origValue = self.m_value;
      currentDelta = 0.;
      currentGeometry = qApp->primaryScreen()->availableGeometry();
    }

    event->accept();
  }

  template <typename T>
  static void mouseMoveEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if ((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      auto delta = (event->screenPos().y() - event->lastScreenPos().y());
      double ratio = qApp->keyboardModifiers() & Qt::CTRL ? .2 : 1.;
      if (std::abs(delta) < 500)
        currentDelta += ratio * delta;

      if (event->screenPos().y() <= 0)
        score::setCursorPos(QPointF(event->screenPos().x(), currentGeometry.height()));
      else if (event->screenPos().y() >= currentGeometry.height())
        score::setCursorPos(QPointF(event->screenPos().x(), 0));

      double v = origValue - currentDelta / currentGeometry.height();
      if (v <= 0.)
      {
        currentDelta = origValue * currentGeometry.height();
        v = 0.;
      }
      else if (v >= 1.)
      {
        currentDelta = (origValue - 1.) * currentGeometry.height();
        v = 1.;
      }

      if (v != self.m_value)
      {
        self.m_value = v;
        self.valueChanged(self.m_value);
        self.sliderMoved();
        self.update();
      }
    }
    event->accept();
  }

  template <typename T>
  static void mouseReleaseEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if (event->button() == Qt::LeftButton)
    {
      score::setCursorPos(event->buttonDownScreenPos(Qt::LeftButton));
      self.unsetCursor();
      if (self.m_grab)
      {
        auto delta = (event->screenPos().y() - event->lastScreenPos().y());
        double ratio = qApp->keyboardModifiers() & Qt::CTRL ? .2 : 1.;
        if (std::abs(delta) < 500)
          currentDelta += ratio * delta;

        double v = origValue - currentDelta / currentGeometry.height();
        double curPos = ossia::clamp(v, 0., 1.);
        if (curPos != self.m_value)
        {
          self.m_value = curPos;
          self.valueChanged(self.m_value);
          self.update();
        }
        self.m_grab = false;
      }
      self.sliderReleased();
    }
    else if (event->button() == Qt::RightButton)
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
      auto obj = self.scene()->addWidget(w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
      obj->setPos(pos);

      QTimer::singleShot(0, w, [w] { w->setFocus(); });

      auto con = QObject::connect(
          w, SignalUtils::QDoubleSpinBox_valueChanged_double(), &self, [&self](double v) {
            self.m_value = self.unmap(v);
            self.valueChanged(self.m_value);
            self.sliderMoved();
            self.update();
          });

      QObject::connect(
          w, &DoubleSpinboxWithEnter::editingFinished, &self, [obj, con, self_p]() mutable {
            if (obj != nullptr)
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
