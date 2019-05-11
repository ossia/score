#pragma once
#include <score/widgets/SignalUtils.hpp>

#include <QGraphicsPixmapItem>
#include <QGraphicsSceneEvent>

#include <cmath>
#include <score_lib_base_export.h>
#include <wobjectdefs.h>

namespace score
{
struct DefaultGraphicsSliderImpl;
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapButton final
    : public QObject,
      public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapButton)
  const QPixmap m_pressed, m_released;

public:
  QGraphicsPixmapButton(
      QPixmap pressed,
      QPixmap released,
      QGraphicsItem* parent);

public:
  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapToggle final
    : public QObject,
      public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapToggle)
  Q_INTERFACES(QGraphicsItem)

  const QPixmap m_pressed, m_released;
  bool m_toggled{};

public:
  QGraphicsPixmapToggle(
      QPixmap pressed,
      QPixmap released,
      QGraphicsItem* parent);

  void toggle();
  void setState(bool toggled);

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsSlider final : public QObject,
                                                    public QGraphicsItem
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

  static constexpr double map(double v) { return v; }
  static constexpr double unmap(double v) { return v; }

  void setRect(const QRectF& r);
  void setValue(double v);
  double value() const;

  bool moving = false;

public:
  void valueChanged(double arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

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

class SCORE_LIB_BASE_EXPORT QGraphicsLogSlider final : public QObject,
                                                       public QGraphicsItem
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
  void valueChanged(double arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

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

class SCORE_LIB_BASE_EXPORT QGraphicsIntSlider final : public QObject,
                                                       public QGraphicsItem
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
  void valueChanged(int arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

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

class SCORE_LIB_BASE_EXPORT QGraphicsComboSlider final : public QObject,
                                                         public QGraphicsItem
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
      const std::array<const char*, N>& arr,
      QGraphicsItem* parent)
      : QGraphicsItem{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);

    this->setAcceptedMouseButtons(Qt::LeftButton);
  }

  QGraphicsComboSlider(QStringList arr, QGraphicsItem* parent)
      : QGraphicsItem{parent}, array{std::move(arr)}
  {
    this->setAcceptedMouseButtons(Qt::LeftButton);
  }

  QGraphicsComboSlider(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;

  bool moving = false;

public:
  void valueChanged(int arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

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
