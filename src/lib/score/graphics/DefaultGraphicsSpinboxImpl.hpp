#pragma once
#include <score/graphics/DefaultControlImpl.hpp>
#include <score/graphics/RightClickWidget.hpp>
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

  // Inverse of the mapping below: the accumulated drag that lands exactly on
  // `bound`. speed is 1 + |delta|, so the value is orig - (1 + |d|) * d / h and
  // d follows from the quadratic.
  static double deltaFor(double orig, double bound, double height) noexcept
  {
    const double k = (orig - bound) * height;
    return k >= 0. ? (-1. + std::sqrt(1. + 4. * k)) / 2.
                   : (1. - std::sqrt(1. - 4. * k)) / 2.;
  }

  template <typename T>
  static double mapValue(T& self, QGraphicsSceneMouseEvent* event) noexcept
  {
    InfiniteScroller::move_free(event);
    const auto speed
        = std::pow(10., std::log10(1. + std::abs(InfiniteScroller::currentDelta)));

    const double height = double(InfiniteScroller::currentGeometry.height());
    auto v = InfiniteScroller::origValue - speed * InfiniteScroller::currentDelta / height;

    // Hold the accumulator at the end rather than letting it run past: a drag
    // that overshoots would otherwise have to be walked all the way back before
    // the value moved again.
    const double bounded = std::clamp(v, double(self.min), double(self.max));
    if(bounded != v)
    {
      InfiniteScroller::currentDelta
          = deltaFor(InfiniteScroller::origValue, bounded, height);
      v = bounded;
    }

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
    if(self.m_grab)
    {
      if(const auto v = mapValue(self, event); v != self.m_value)
      {
        self.m_value = v;
        self.update();
      }
      InfiniteScroller::stop(self, event);
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

  //! QEvent::UngrabMouse: the scene took the implicit grab away and there will
  //! be no release to end the drag on. See DefaultGraphicsSliderImpl for what
  //! goes wrong if the edit is left open.
  template <typename T>
  static void ungrabMouseEvent(T& self, QEvent* event)
  {
    if(!self.m_grab)
      return;

    self.m_grab = false;
    InfiniteScroller::abort(self);
    if(self.m_noValueChangeOnMove)
      self.sliderMoved();
    self.sliderReleased();
  }

  template <typename T>
    requires std::is_integral_v<std::decay_t<decltype(std::declval<T>().value())>>
  static void contextMenuEvent(T& self, QPointF pos)
  {
    auto build = [&, self_p = &self, pos] {
      // Whatever box is open belongs to the previous right-click.
      closeRightClickWidget();

      auto w = new SpinboxWithEnter;
      w->setRange(self.min, self.max);

      w->setValue(self.map(self.m_value));
      auto obj = self.scene()->addWidget(
          w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
      obj->setPos(pos);
      currentRightClickWidget() = obj;

#if defined(__EMSCRIPTEN__)
      w->setFocus();
#else
      QTimer::singleShot(0, w, [w] { w->setFocus(); });
#endif

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
    };
#if defined(__EMSCRIPTEN__)
    build();
#else
    QTimer::singleShot(0, &self, build);
#endif
  }

  template <typename T>
    requires std::is_floating_point_v<std::decay_t<decltype(std::declval<T>().value())>>
  static void contextMenuEvent(T& self, QPointF pos)
  {
    // FIXME to be safe we have to locate the object by path on every click as
    // some control changes may cause entire GUI rebuilds
    auto build = [&, self_p = &self, pos] {
      // Whatever box is open belongs to the previous right-click.
      closeRightClickWidget();

      auto w = new DoubleSpinboxWithEnter;
      w->setRange(self.min, self.max);

      w->setDecimals(6);
      w->setValue(self.map(self.m_value));
      auto obj = self.scene()->addWidget(
          w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
      obj->setPos(pos);
      currentRightClickWidget() = obj;

#if defined(__EMSCRIPTEN__)
      w->setFocus();
#else
      QTimer::singleShot(0, w, [w] { w->setFocus(); });
#endif

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
    };
#if defined(__EMSCRIPTEN__)
    build();
#else
    QTimer::singleShot(0, &self, build);
#endif
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
