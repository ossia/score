#pragma once
#include <score/widgets/DoubleSpinBox.hpp>
#include <score/widgets/Pixmap.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsPixmapItem>
#include <QGraphicsProxyWidget>
#include <QPointer>

#include <cmath>
#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
static const constexpr QRectF defaultSliderSize{0., 0., 60., 20.};
static const constexpr QRectF defaultKnobSize{0., 0., 35., 35.};
static const constexpr QRectF defaultCheckBoxSize{0., 0., 20., 20.};
static const constexpr QRectF defaultToggleSize{0., 0., 60., 20.};

struct DoubleSpinboxWithEnter;
struct DefaultGraphicsSliderImpl;

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapButton final : public QObject,
                                                          public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapButton)
  const QPixmap m_pressed, m_released;

public:
  QGraphicsPixmapButton(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

public:
  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsSelectablePixmapToggle final : public QObject,
                                                                    public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsSelectablePixmapToggle)
  Q_INTERFACES(QGraphicsItem)

  const QPixmap m_pressed, m_pressed_selected, m_released, m_released_selected;
  bool m_toggled{};
  bool m_selected{};

public:
  QGraphicsSelectablePixmapToggle(
      QPixmap pressed,
      QPixmap pressed_selected,
      QPixmap released,
      QPixmap released_selected,
      QGraphicsItem* parent);

  void toggle();
  void setSelected(bool selected);
  void setState(bool toggled);

  bool state() const noexcept { return m_toggled; }

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapToggle final : public QObject,
                                                          public QGraphicsPixmapItem
{
  W_OBJECT(QGraphicsPixmapToggle)
  Q_INTERFACES(QGraphicsItem)

  const QPixmap m_pressed, m_released;
  bool m_toggled{};

public:
  QGraphicsPixmapToggle(QPixmap pressed, QPixmap released, QGraphicsItem* parent);

  void toggle();
  void setState(bool toggled);

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};

template <typename T>
struct SCORE_LIB_BASE_EXPORT QGraphicsSliderBase : public QGraphicsItem
{
  QGraphicsSliderBase(QGraphicsItem* parent);
  ~QGraphicsSliderBase();

  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;

  void setRect(const QRectF& r);
  QRectF boundingRect() const override;

  QRectF m_rect{defaultSliderSize};
  QPointer<DoubleSpinboxWithEnter> spinbox{};
  QPointer<QGraphicsProxyWidget> spinboxProxy{};
};

class SCORE_LIB_BASE_EXPORT QGraphicsSlider final : public QObject,
                                                    public QGraphicsSliderBase<QGraphicsSlider>
{
  W_OBJECT(QGraphicsSlider)
  friend struct DefaultGraphicsSliderImpl;
  friend struct QGraphicsSliderBase<QGraphicsSlider>;

  double m_value{};

public:
  double min{}, max{};

private:
  bool m_grab{};

public:
  QGraphicsSlider(QGraphicsItem* parent);

  double unmap(double v) const noexcept { return (v - min) / (max - min); }
  double map(double v) const noexcept { return (v * (max - min)) + min; }

  void setRange(double min, double max);
  void setValue(double v);
  double value() const;

  bool moving = false;

public:
  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsKnob final : public QObject, public QGraphicsItem
{
  W_OBJECT(QGraphicsKnob)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsKnobImpl;

  double m_value{};
  QRectF m_rect{defaultKnobSize};

public:
  double min{}, max{};

private:
  bool m_grab{};

public:
  QGraphicsKnob(QGraphicsItem* parent);

  double unmap(double v) const noexcept { return (v - min) / (max - min); }
  double map(double v) const noexcept { return (v * (max - min)) + min; }

  void setRect(const QRectF& r);
  void setRange(double min, double max);
  void setValue(double v);
  double value() const;
  QRectF boundingRect() const override;

  bool moving = false;

public:
  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsLogKnob final : public QObject, public QGraphicsItem
{
  W_OBJECT(QGraphicsLogKnob)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsKnobImpl;

  double m_value{};
  QRectF m_rect{defaultKnobSize};

public:
  double min{}, max{};

private:
  bool m_grab{};

public:
  QGraphicsLogKnob(QGraphicsItem* parent);

  double map(double v) const noexcept;
  double unmap(double v) const noexcept;

  void setRect(const QRectF& r);
  void setRange(double min, double max);
  void setValue(double v);
  double value() const;
  QRectF boundingRect() const override;

  bool moving = false;

public:
  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsLogSlider final
    : public QObject,
      public QGraphicsSliderBase<QGraphicsLogSlider>
{
  W_OBJECT(QGraphicsLogSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;
  friend struct QGraphicsSliderBase<QGraphicsLogSlider>;

  double m_value{};

public:
  double min{}, max{};

private:
  bool m_grab{};

public:
  QGraphicsLogSlider(QGraphicsItem* parent);

  void setRange(double min, double max);
  void setValue(double v);
  double value() const;

  double map(double v) const noexcept;
  double unmap(double v) const noexcept;

  bool moving = false;

public:
  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsIntSlider final
    : public QObject,
      public QGraphicsSliderBase<QGraphicsIntSlider>
{
  W_OBJECT(QGraphicsIntSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultGraphicsSliderImpl;
  friend struct QGraphicsSliderBase<QGraphicsIntSlider>;
  int m_value{}, m_min{}, m_max{};
  bool m_grab{};

public:
  QGraphicsIntSlider(QGraphicsItem* parent);

  void setValue(int v);
  void setRange(int min, int max);
  int value() const;

  bool moving = false;

public:
  void valueChanged(int arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
  double getHandleX() const;
};

class SCORE_LIB_BASE_EXPORT QGraphicsCombo final : public QObject, public QGraphicsItem
{
  W_OBJECT(QGraphicsCombo)
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultComboImpl;
  QRectF m_rect{defaultSliderSize};

public:
  QStringList array;

private:
  int m_value{};
  bool m_grab{};

public:
  template <std::size_t N>
  QGraphicsCombo(const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);
  }

  QGraphicsCombo(QStringList arr, QGraphicsItem* parent) : QGraphicsCombo{parent}
  {
    array = std::move(arr);
  }

  QGraphicsCombo(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;

  bool moving = false;

public:
  void valueChanged(int arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsEnum : public QObject, public QGraphicsItem
{
  W_OBJECT(QGraphicsEnum)
  Q_INTERFACES(QGraphicsItem)

protected:
  int m_value{};
  int m_clicking{-1};
  QRectF m_rect;
  QRectF m_smallRect;

public:
  std::vector<QString> array;
  int rows{1};
  int columns{4};

  template <std::size_t N>
  QGraphicsEnum(const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsEnum{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);
  }
  QGraphicsEnum(std::vector<QString> arr, QGraphicsItem* parent) : QGraphicsEnum{parent}
  {
    array = std::move(arr);
  }
  QGraphicsEnum(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;
  QRectF boundingRect() const override;

public:
  void currentIndexChanged(int arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, currentIndexChanged, arg_1)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsPixmapEnum final : public QGraphicsEnum
{
public:
  std::vector<QPixmap> on_images;
  std::vector<QPixmap> off_images;

  template <std::size_t N>
  QGraphicsPixmapEnum(
      const std::array<const char*, N>& arr,
      const std::array<const char*, 2 * N>& pixmaps,
      QGraphicsItem* parent)
      : QGraphicsPixmapEnum{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);

    for (std::size_t i = 0; i < pixmaps.size(); i++)
    {
      if (i % 2)
        off_images.emplace_back(score::get_pixmap(pixmaps[i]));
      else
        on_images.emplace_back(score::get_pixmap(pixmaps[i]));
    }
  }

  QGraphicsPixmapEnum(
      std::vector<QString> arr,
      const std::vector<QString>& pixmaps,
      QGraphicsItem* parent)
      : QGraphicsPixmapEnum{parent}
  {
    array = std::move(arr);

    for (std::size_t i = 0; i < pixmaps.size(); i++)
    {
      if (i % 2)
        off_images.emplace_back(score::get_pixmap(pixmaps[i]));
      else
        on_images.emplace_back(score::get_pixmap(pixmaps[i]));
    }
  }

  QGraphicsPixmapEnum(QGraphicsItem* parent);

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsHSVChooser final : public QObject, public QGraphicsItem
{
  W_OBJECT(QGraphicsHSVChooser)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{0., 0., 140., 100.};

private:
  double h{}, s{}, v{};
  ossia::vec4f m_value{};
  bool m_grab{};

public:
  QGraphicsHSVChooser(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(ossia::vec4f v);
  ossia::vec4f value() const;

  bool moving = false;

public:
  void valueChanged(ossia::vec4f arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsXYChooser final : public QObject, public QGraphicsItem
{
  W_OBJECT(QGraphicsXYChooser)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{0., 0., 100., 100.};

private:
  ossia::vec2f m_value{};
  bool m_grab{};

public:
  QGraphicsXYChooser(QGraphicsItem* parent);

  void setPoint(const QPointF& r);
  void setValue(ossia::vec2f v);
  ossia::vec2f value() const;

  bool moving = false;

public:
  void valueChanged(ossia::vec2f arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsCheckBox final
    : public QObject,
      public QGraphicsItem
{
  W_OBJECT(QGraphicsCheckBox)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{defaultCheckBoxSize};

   bool m_toggled{};

public:
  QGraphicsCheckBox(QGraphicsItem* parent);

  void toggle();
  void setState(bool toggled);

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};


class SCORE_LIB_BASE_EXPORT QGraphicsToggle final
    : public QObject,
      public QGraphicsItem
{
  W_OBJECT(QGraphicsToggle)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{defaultToggleSize};

  QString m_textToggled{};
  QString m_textUntoggled{};
  bool m_toggled{};

public:
  QGraphicsToggle(const QString& textToggled, const QString& textUntoggled, QGraphicsItem* parent);

  void toggle();
  void setState(bool toggled);

public:
  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsButton final
    : public QObject,
      public QGraphicsItem
{
  W_OBJECT(QGraphicsButton)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{defaultToggleSize};

  bool m_pressed{};

public:
  QGraphicsButton(QGraphicsItem* parent);

  void bang();

  void pressed(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, pressed, b)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
}
