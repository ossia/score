#pragma once
#include <score/graphics/DefaultControlImpl.hpp>
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
#include <QPointer>
#include <QScreen>
#include <QTimer>

namespace score
{
struct DefaultGraphicsSpinboxImpl
{
  template <typename T>
  static void paint(
      T& self, const score::Skin& skin, const QString& text, QPainter* painter,
      QWidget* widget)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setPen(skin.NoPen);
    painter->setBrush(skin.Emphasis2.main.brush);

    // Draw rect
    const QRectF brect = self.boundingRect();
    painter->drawRoundedRect(brect, 1, 1);

    // Draw text
    painter->setPen(skin.Base4.main.pen1);
    painter->setFont(skin.Medium8Pt);
    const auto textrect = brect.adjusted(2, 3, -2, -2);
    painter->drawText(textrect, text, QTextOption(Qt::AlignLeft));

    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  template <typename T>
  static void mousePressEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton)
    {
      self.m_grab = true;
      InfiniteScroller::start(self, (self.max - self.min) * self.m_value + self.min);
    }

    event->accept();
  }

  template <typename T>
  static double mapValue(T& self, QGraphicsSceneMouseEvent* event) noexcept
  {
    InfiniteScroller::move_free(event);
    const auto speed
        = std::pow(10., std::log10(1. + std::abs(InfiniteScroller::currentDelta)));

    auto v = InfiniteScroller::origValue
             - speed * InfiniteScroller::currentDelta
                   / double(InfiniteScroller::currentGeometry.height());
    v = (v - self.min) / (self.max - self.min);
    return std::clamp(v, 0., 1.);
  }

  template <typename T>
  static void mouseMoveEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      if(const auto v = mapValue(self, event); v != self.m_value)
      {
        self.m_value = v;
        if(!self.m_noValueChangeOnMove)
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
      if(const auto v = mapValue(self, event); v != self.m_value)
      {
        self.m_value = v;
        self.update();
      }
    }

    if(self.m_noValueChangeOnMove)
      self.sliderMoved();
    self.m_grab = false;
    self.sliderReleased();

    if(event->button() == Qt::RightButton)
    {
      contextMenuEvent(self, event->scenePos());
    }

    event->accept();
  }

  template <typename T>
    requires std::is_integral_v<std::decay_t<decltype(std::declval<T>().value())>>
  static void contextMenuEvent(T& self, QPointF pos)
  {
    QTimer::singleShot(0, &self, [&, self_p = &self, pos] {
      auto w = new SpinboxWithEnter;
      w->setRange(self.min, self.max);

      w->setValue(self.map(self.m_value));
      auto obj = self.scene()->addWidget(
          w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
      obj->setPos(pos);

      QTimer::singleShot(0, w, [w] { w->setFocus(); });

      auto con = QObject::connect(
          w, SignalUtils::QSpinBox_valueChanged_int(), &self,
          [&self, obj, scene = self.scene()](double v) {
        DefaultControlImpl::editWidgetInContextMenu(self, scene, obj, v);
      });

      QObject::connect(
          w, &SpinboxWithEnter::editingFinished, &self, [obj, con, self_p]() mutable {
        if(obj != nullptr)
        {
          if(self_p->m_noValueChangeOnMove)
            self_p->sliderMoved();
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
    requires std::is_floating_point_v<std::decay_t<decltype(std::declval<T>().value())>>
  static void contextMenuEvent(T& self, QPointF pos)
  {
    // FIXME to be safe we have to locate the object by path on every click as
    // some control changes may cause entire GUI rebuilds
    QTimer::singleShot(0, &self, [&, self_p = &self, pos] {
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
          [&self, obj, scene = self.scene()](double v) {
        DefaultControlImpl::editWidgetInContextMenu(self, scene, obj, v);
      });

      QObject::connect(
          w, &DoubleSpinboxWithEnter::editingFinished, &self,
          [obj, con, self_p]() mutable {
        if(obj != nullptr)
        {
          if(self_p->m_noValueChangeOnMove)
            self_p->sliderMoved();
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
    self.m_value = self.unmap(self.init);

    self.m_grab = true;
    self.sliderMoved();
    self.sliderReleased();
    self.m_grab = false;

    self.update();

    event->accept();
  }
};
}
