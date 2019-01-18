#pragma once
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPainter>
#include <QEventLoop>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>

#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/math.hpp>
namespace score
{

struct DoubleSpinboxWithEnter final : public QDoubleSpinBox
{
  W_OBJECT(DoubleSpinboxWithEnter)
public:
  using QDoubleSpinBox::QDoubleSpinBox;

  void ok() W_SIGNAL(ok);
public:
  bool event(QEvent* event) override
  {
    if (event->type() == QEvent::ShortcutOverride)
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      switch(keyEvent->key())
      {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Escape:
          editingFinished();
      default:
          break;
      }
    }
    return QDoubleSpinBox::event(event);
  }
};

struct DefaultGraphicsSliderImpl
{
  template <typename T, typename U>
  static void paint(
      T& self, const U& skin, const QString& text, QPainter* painter,
      QWidget* widget)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);

    static const QPen darkPen{skin.HalfDark.color()};
    static const QPen grayPen{skin.Gray.color()};
    painter->setPen(darkPen);
    painter->setBrush(skin.Dark);

    // Draw rect
    const auto srect = self.sliderRect();
    painter->drawRoundedRect(srect, 1, 1);

    // Draw text
#if defined(__linux__)
    static const auto dpi_adjust = widget->devicePixelRatioF() > 1 ? 0 : -1;
#elif defined(_WIN32)
    static const constexpr auto dpi_adjust = -4;
#else
    static const constexpr auto dpi_adjust = -2;
#endif
    painter->setPen(grayPen);
    painter->setFont(skin.SansFontSmall);
    painter->drawText(
        srect.adjusted(6, dpi_adjust, -6, -1), text,
        self.getHandleX() > srect.width() / 2 ? QTextOption()
                                              : QTextOption(Qt::AlignRight));

    // Draw handle
    painter->setBrush(skin.HalfLight);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->drawRect(self.handleRect());
  }

  template <typename T>
  static void mousePressEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton)
    {
      if (self.isInHandle(event->pos()))
      {
        self.m_grab = true;
      }

      const auto srect = self.sliderRect();
      double curPos
          = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
      if (curPos != self.m_value)
      {
        self.m_value = curPos;
        self.valueChanged(self.m_value);
        self.sliderMoved();
        self.update();
      }
    }


    event->accept();
  }

  template <typename T>
  static void mouseMoveEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if ((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      const auto srect = self.sliderRect();
      double curPos
          = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
      if (curPos != self.m_value)
      {
        self.m_value = curPos;
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
      if (self.m_grab)
      {
        double curPos
            = ossia::clamp(event->pos().x() / self.sliderRect().width(), 0., 1.);
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
      const auto pos = event->scenePos();
      QTimer::singleShot(0, [&,pos=event->scenePos()] {
        auto w = new DoubleSpinboxWithEnter;
        w->setRange(self.map(self.min), self.map(self.max));

        w->setDecimals(6);
        w->setValue(self.map(self.m_value * (self.max - self.min) + self.min));
        auto obj = self.scene()->addWidget(
            w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        obj->setPos(pos);

        QTimer::singleShot(0, w, [w] { w->setFocus(); });

        QObject::connect(
          w, SignalUtils::QDoubleSpinBox_valueChanged_double(), &self,
          [=, &self](double v) {
            self.m_value = (self.unmap(v) - self.min) / (self.max - self.min);
            self.valueChanged(self.m_value);
            self.sliderMoved();
            self.update();
          });

        QObject::connect(
          w, &DoubleSpinboxWithEnter::editingFinished, &self, [=, &self] {
            self.sliderReleased();
            QTimer::singleShot(0, obj, [scene = self.scene(), obj] {
              scene->removeItem(obj);
              delete obj;
            });
        });
      });
    }
    event->accept();
  }

  template <typename T>
  static void mouseDoubleClickEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
  }
};


}
