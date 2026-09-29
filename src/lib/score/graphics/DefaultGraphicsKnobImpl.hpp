#pragma once
#include <score/graphics/WidgetPresentation.hpp>
#include <score/graphics/DefaultControlImpl.hpp>
#include <score/graphics/RightClickWidget.hpp>
#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/Constants.hpp>
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
struct DefaultGraphicsKnobImpl
{
  template <typename T>
  static void paint(
      T& self, const score::Skin& skin, const QString& text, QPainter* painter,
      QWidget* widget, const QStyleOptionGraphicsItem* option = nullptr)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);

    constexpr const double space = 50.;
    constexpr const double start = (270. - space) * 16.;
    constexpr const double totalSpan = (360. - 2. * space) * 16.;

    // Only knobs have m_rect (WidgetPresentation); e.g. the time chooser does not.
    QRectF srect = defaultKnobSize;
    if constexpr(requires { self.m_rect; })
      srect = self.m_rect;
    // Taller than wide: dial in the top square, value below
    const double side = std::min(srect.width(), srect.height());

    // Geometry of the design at the 35 px reference size: the arc's centre
    // line at a 10.5 px radius, 2 px wide; the tick from the arc's outer
    // edge (11.5 px) to 4.94 px from the centre, 2 px wide, round ends; the
    // body filled out to the arc's outer edge, so the arc lies on its rim. A
    // round cap reaches half the pen width past its endpoint: the endpoints
    // are pulled in so the ends are at those radii.
    const double k = side / 35.;
    const QPointF c{side / 2., side / 2.};
    const double arcRadius = 10.5 * k;
    // The arc and the tick share one pen
    const double arcWidth = std::max(1., 2. * k);
    const double tickWidth = arcWidth;
    // Antialiased, a round end still bleeds past its geometric edge by a
    // fraction of a device pixel more than the arc's own edge does, most at
    // small zooms: half a device pixel of margin keeps it inside the rim.
    const QTransform& dt = painter->deviceTransform();
    const double devicePx
        = 1. / std::max(1e-6, std::sqrt(std::abs(dt.m11() * dt.m22() - dt.m12() * dt.m21())));
    const double tickOuter
        = arcRadius + arcWidth / 2. - tickWidth / 2. - 0.5 * devicePx;
    const double tickInner = 4.94 * k + tickWidth / 2.;
    const QRectF r{
        c.x() - arcRadius, c.y() - arcRadius, 2. * arcRadius, 2. * arcRadius};
    const double bodyRadius = arcRadius + arcWidth / 2.;
    const QRectF body{
        c.x() - bodyRadius, c.y() - bodyRadius, 2. * bodyRadius, 2. * bodyRadius};

    const QColor accent = skin.Base4.main.brush.color();

    // Body
    painter->setPen(Qt::NoPen);
    painter->setBrush(skin.Emphasis2.main.brush);
    painter->drawChord(body, start, -totalSpan);
    painter->setBrush(Qt::NoBrush);

    // The value; no track around the rest of the course (the body shows it)
    const double valueSpan = -self.m_value * totalSpan;
    painter->setPen(QPen{accent, arcWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
    if(self.m_value > 0.)
      painter->drawArc(r, start, valueSpan);

    // Tick, joined to the arc
    const double theta = -0.0174533 * (start + valueSpan) / 16.;
    const QPointF dir{std::cos(theta), std::sin(theta)};
    painter->drawLine(c + tickInner * dir, c + tickOuter * dir);

    // Where the execution is, inside the arc
    if(self.m_hasExec)
    {
      painter->setPen(skin.Base4.lighter180.pen1);
      const double er = arcRadius - arcWidth / 2. - 1.;
      const QRectF erect{c.x() - er, c.y() - er, 2. * er, 2. * er};
      painter->drawArc(erect, start, -self.m_execValue * totalSpan);
    }

    const double textDelta = side >= 30. ? -10. : side >= 20. ? -9. : -8.;
    // The value in the same colour as under a slider.
    painter->setPen(skin.Base4.lighter180.pen1);

    // Draw text
    // Non-item wrappers (multi-slider rows) always show their value
    bool showValue = true;
    if constexpr(std::is_base_of_v<QGraphicsItem, std::remove_cvref_t<T>>)
    {
      bool grabbed = false;
      if constexpr(requires { self.m_grab; })
        grabbed = self.m_grab;
      showValue = showsValue(self, grabbed, option);
    }
    if(showValue && !text.isEmpty())
    {
      painter->setFont(skin.Medium8Pt);
      const double text_y
          = srect.height() > srect.width() ? side : srect.height() + textDelta;
      painter->drawText(
          QRectF{0., text_y, srect.width(), 10.}, text, QTextOption(Qt::AlignCenter));
    }

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
    // Read the value before ending the session: once the relative-motion
    // session is over, move() falls back to pointer positions, and under a
    // pointer lock those never left the press point -- applying that as a delta
    // unwinds the whole drag.
    if(self.m_grab)
    {
      double v = InfiniteScroller::move(event);
      if(v != self.m_value)
      {
        self.m_value = v;
        self.update();
      }
      InfiniteScroller::stop(self, event);
    }

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

  //! A value typed in as a real number, map(m_value): a scalar control, or one
  //! whose value() is more than the number (the time chooser's {time, sync}).
  template <typename T>
    requires(!std::is_integral_v<std::decay_t<decltype(std::declval<T>().value())>>)
  static void contextMenuEvent(T& self, QPointF pos)
  {
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
