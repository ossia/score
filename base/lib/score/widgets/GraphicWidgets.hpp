#pragma once
#include <ossia/detail/math.hpp>
#include <wobjectdefs.h>

#include <QGraphicsPixmapItem>
#include <QGraphicsSceneEvent>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPainter>
#include <score/widgets/SignalUtils.hpp>
#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapButton final
    : public QObject
    , public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapButton)
  const QPixmap m_pressed, m_released;

public:
  QGraphicsPixmapButton(
      QPixmap pressed, QPixmap released, QGraphicsItem* parent);

public:
  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapToggle final
    : public QObject
    , public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapToggle)
  Q_INTERFACES(QGraphicsItem)

  const QPixmap m_pressed, m_released;
  bool m_toggled{};

public:
  QGraphicsPixmapToggle(
      QPixmap pressed, QPixmap released, QGraphicsItem* parent);

  void toggle();
  void setState(bool toggled);

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

struct DefaultGraphicsSliderImpl
{
  struct DoubleSpinboxWithEnter final : public QDoubleSpinBox
  {
  public:
    using QDoubleSpinBox::QDoubleSpinBox;
  public:
    bool event(QEvent* event) override
    {
      if(event->type() == QEvent::ShortcutOverride)
      {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if(keyEvent->key() == Qt::Key_Return)
        {
          editingFinished();
        }
      }
      return QDoubleSpinBox::event(event);
    }
  };

  template<typename T, typename U>
  static void paint(T& self, const U& skin, const QString& text, QPainter* painter, QWidget* widget)
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
  #elif defined(_MSC_VER)
    static const constexpr auto dpi_adjust = -4;
  #else
    static const constexpr auto dpi_adjust = -2;
  #endif
    painter->setPen(grayPen);
    painter->setFont(skin.SansFontSmall);
    painter->drawText(
        srect.adjusted(6, dpi_adjust, -6, -1),
        text,
        self.getHandleX() > srect.width() / 2 ? QTextOption()
                                         : QTextOption(Qt::AlignRight));

    // Draw handle
    painter->setBrush(skin.HalfLight);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->drawRect(self.handleRect());
  }

  template<typename T>
  static void mousePressEvent(T& self, QGraphicsSceneMouseEvent* event)
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

    event->accept();
  }

  template<typename T>
  static void mouseMoveEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    if (self.m_grab)
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

  template<typename T>
  static void mouseReleaseEvent(T& self, QGraphicsSceneMouseEvent* event)
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
    event->accept();
  }

  template<typename T>
  static void mouseDoubleClickEvent(T& self, QGraphicsSceneMouseEvent* event)
  {
    auto w = new DoubleSpinboxWithEnter;
    w->setRange(self.map(self.min), self.map(self.max));

    w->setDecimals(6);
    w->setValue(self.map(self.m_value * (self.max - self.min) + self.min));
    auto obj = self.scene()->addWidget(w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    obj->setPos(event->scenePos());
    w->setFocus();

    QObject::connect(w, SignalUtils::QDoubleSpinBox_valueChanged_double(),
            &self, [=,&self] (double v) {
      self.m_value = (self.unmap(v) - self.min) / (self.max - self.min);
      self.valueChanged(self.m_value);
      self.sliderMoved();
      self.update();
    });
    QObject::connect(w, &QDoubleSpinBox::editingFinished,
                     &self, [=,&self] {
      self.scene()->removeItem(obj); obj->deleteLater();
    });
  }
};

class SCORE_LIB_BASE_EXPORT QGraphicsSlider final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;

  double m_value{};
  QRectF m_rect{};

public:
  double min{}, max{};

private:
  bool m_grab{};

public:
  QGraphicsSlider(QGraphicsItem* parent);

  static double map(double v) { return v; }
  static double unmap(double v) { return v; }

  void setRect(const QRectF& r);
  void setValue(double v);
  double value() const;

  bool moving = false;
public:
  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1);
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved);
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;
};

class SCORE_LIB_BASE_EXPORT QGraphicsLogSlider final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsLogSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;

  double m_value{};
  QRectF m_rect{};

public:
  double min{}, max{};

private:
  bool m_grab{};

public:
  QGraphicsLogSlider(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(double v);
  double value() const;

  static double map(double v) { return std::exp2(v); }
  static double unmap(double v) { return std::log2(v); }

  bool moving = false;
public:
  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1);
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved);
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;
};

class SCORE_LIB_BASE_EXPORT QGraphicsIntSlider final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsIntSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;
  QRectF m_rect{};
  int m_value{}, m_min{}, m_max{};
  bool m_grab{};

public:
  QGraphicsIntSlider(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(int v);
  void setRange(int min, int max);
  int value() const;

  bool moving = false;
public:
  void valueChanged(int arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1);
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved);
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;
};

class SCORE_LIB_BASE_EXPORT QGraphicsComboSlider final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsComboSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;
  QRectF m_rect{};

public:
  QStringList array;

private:
  int m_value{};
  bool m_grab{};

public:
  template <std::size_t N>
  QGraphicsComboSlider(
      const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsItem{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);

    this->setAcceptedMouseButtons(Qt::LeftButton);
  }

  QGraphicsComboSlider(
      QStringList arr, QGraphicsItem* parent)
      : QGraphicsItem{parent}
      , array{std::move(arr)}
  {
    this->setAcceptedMouseButtons(Qt::LeftButton);
  }

  QGraphicsComboSlider(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;

  bool moving = false;
public:
  void valueChanged(int arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1);
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved);
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;
};
}
