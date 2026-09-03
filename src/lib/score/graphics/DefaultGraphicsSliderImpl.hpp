#pragma once
#include <score/graphics/DefaultControlImpl.hpp>
#include <score/graphics/RightClickWidget.hpp>
#include <score/model/Skin.hpp>
#include <score/widgets/DoubleSpinBox.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QTimer>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{

struct RightClickImpl
{
  QPointer<DoubleSpinboxWithEnter> spinbox{};
  QPointer<QGraphicsProxyWidget> spinboxProxy{};
};

struct DefaultGraphicsSliderImpl
{
  template <typename T>
  static void paint(
      T& self, const score::Skin& skin, const QString& text, QPainter* painter,
      QWidget* widget)
  {
    painter->setRenderHint(QPainter::Antialiasing, false);

    painter->setPen(skin.NoPen);
    painter->setBrush(skin.Emphasis2.main.brush);

    // Draw rect
    const QRectF brect = self.boundingRect();
    const QRectF srect = self.sliderRect();
    painter->drawRoundedRect(srect, 1, 1);

    // Draw text
#if defined(__linux__)
    // widget is null when painting through QGraphicsScene::render
    static const auto dpi_adjust
        = (widget ? widget->devicePixelRatioF() : painter->device()->devicePixelRatioF())
                  > 1
              ? 0
              : -1;
#elif defined(_WIN32)
    static const constexpr auto dpi_adjust = 0;
#else
    static const constexpr auto dpi_adjust = -2;
#endif
    painter->setPen(skin.Base4.lighter180.pen1);
    painter->setFont(skin.Medium8Pt);
    const auto textrect = brect.adjusted(2, srect.height() + 2 + dpi_adjust, -2, -1);
    painter->drawText(textrect, text, QTextOption(Qt::AlignCenter));

    // Draw handle
    painter->fillRect(self.handleRect(), skin.Base4);
    if(self.m_hasExec)
      painter->fillRect(self.execHandleRect(), skin.Base4.lighter180.brush);
    // painter->setPen(QPen(Qt::green, 1));
    // painter->setBrush(QBrush(Qt::transparent));
    // painter->drawRect(textrect);
    //
    // painter->setPen(QPen(Qt::red, 1));
    // painter->setBrush(QBrush(Qt::transparent));
    // painter->drawRect(self.boundingRect());
  }

  template <typename T>
  static void mousePressEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton && self.isInHandle(event->pos()))
    {
      self.m_grab = true;

      const auto srect = self.sliderRect();
      const auto posX = event->pos().x() - srect.x();
      double curPos = self.from01(ossia::clamp(posX, 0., srect.width()) / srect.width());
      if(curPos != self.m_value)
      {
        self.m_value = curPos;
        self.sliderMoved();
        self.update();
      }
    }

    event->accept();
  }

  template <typename T>
  static void mouseMoveEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      const auto srect = self.sliderRect();
      const auto posX = event->pos().x() - srect.x();
      double curPos = self.from01(ossia::clamp(posX, 0., srect.width()) / srect.width());
      if(curPos != self.m_value)
      {
        if constexpr(std::is_same_v<decltype(self.m_value), int>)
        {
          self.m_value = curPos;
        }
        else
        {
          double ratio = qGuiApp->keyboardModifiers() & Qt::CTRL ? 0.1 : 1.;
          self.m_value = std::clamp(
              double(self.m_value + ((curPos - self.m_value) * ratio)), 0., 1.);
        }

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
      const auto srect = self.sliderRect();
      const auto posX = event->pos().x() - srect.x();
      double curPos = self.from01(ossia::clamp(posX, 0., srect.width()) / srect.width());
      if(curPos != self.m_value)
      {
        if constexpr(std::is_same_v<decltype(self.m_value), int>)
        {
          self.m_value = curPos;
        }
        else
        {
          double ratio = qGuiApp->keyboardModifiers() & Qt::CTRL ? 0.1 : 1.;
          self.m_value = std::clamp(
              double(self.m_value + ((curPos - self.m_value) * ratio)), 0., 1.);
        }
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

  //! QEvent::UngrabMouse. The scene drops an item's implicit grab without ever
  //! delivering a release -- most visibly when the button was let go outside
  //! the window and the pointer then comes back in, which
  //! QGraphicsScenePrivate::sendMouseEvent answers by clearing the grabber and
  //! discarding the event. The drag has to be closed here too: otherwise
  //! `moving` stays set and this control stops following its model, and the
  //! ongoing command it opened stays open, so the *next* control the user
  //! touches has its edits redirected onto this one.
  template <typename T>
  static void ungrabMouseEvent(T& self, QEvent* event)
  {
    if(!self.m_grab)
      return;

    self.m_grab = false;
    self.sliderReleased();
  }

  template <typename T>
  static void contextMenuEvent(T& self, QPointF pos)
  {
    auto build = [&, self_p = &self, pos] {
      // Whatever was open belongs to the previous right-click, including one
      // on this same control.
      closeRightClickWidget();
      self_p->impl->spinbox = nullptr;
      self_p->impl->spinboxProxy = nullptr;

      auto w = new DoubleSpinboxWithEnter;
      self.impl->spinbox = w;
      w->setRange(self.min, self.max);

      w->setDecimals(6);
      w->setValue(self.map(self.m_value));
      auto obj = self.scene()->addWidget(
          w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
      obj->setPos(pos);
      self.impl->spinboxProxy = obj;
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
          w, &DoubleSpinboxWithEnter::editingFinished, &self, [obj, con, self_p] {
        if(self_p->impl->spinbox)
        {
          self_p->sliderReleased();
          QObject::disconnect(con);
          // obj is the timer's context, not self: if the next right-click
          // took this box down first, through closeRightClickWidget(), the
          // teardown goes with it rather than freeing it a second time.
          QTimer::singleShot(0, obj, [scene = self_p->scene(), obj] {
            scene->removeItem(obj);
            delete obj;
          });
          self_p->impl->spinbox = nullptr;
          self_p->impl->spinboxProxy = nullptr;
        }
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
