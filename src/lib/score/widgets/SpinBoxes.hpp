#pragma once

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimeEdit>
#include <QWheelEvent>
#include <type_traits>
#include <ossia-qt/time.hpp>
#include <score_lib_base_export.h>
class QStyleOptionFrame;
namespace score
{
/**
 * @brief The TimeSpinBox class
 *
 * Adapted for the score usage in various duration widgets.
 */

struct BarSpinBox;
struct SecondSpinBox;
struct FlicksSpinBox;
class SCORE_LIB_BASE_EXPORT TimeSpinBox final : public QWidget
{
  W_OBJECT(TimeSpinBox)
public:
  TimeSpinBox(QWidget* parent = nullptr);
  ~TimeSpinBox();

  enum TimeMode {
    Bars,
    Seconds,
    Flicks
  };
  static void setGlobalTimeMode(TimeMode);

  void setMinimumTime(ossia::time_value t);
  void setMaximumTime(ossia::time_value t);
  void setTime(ossia::time_value t);
  ossia::time_value time() const noexcept;
  void timeChanged(ossia::time_value t) E_SIGNAL(SCORE_LIB_BASE_EXPORT, timeChanged, t)
  void editingFinished() E_SIGNAL(SCORE_LIB_BASE_EXPORT, editingFinished)
  void wheelEvent(QWheelEvent* event) override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void initStyleOption(QStyleOptionFrame *option) const noexcept;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

private:
  void updateTime();
  QPoint m_startPos{};
  int64_t m_prevY{};
  int64_t m_travelledY{};

  int64_t m_origFlicks{};

  int64_t m_flicks{};
  int64_t m_min{};
  int64_t m_max{};

  ossia::bar_time m_barTime{};

  enum GrabbedHandle {
    None, Bar, Quarter, Semiquaver, Cent
  } m_grab{None};

  TimeMode m_mode{Bars};

  friend struct BarSpinBox;
  friend struct SecondSpinBox;
  friend struct FlicksSpinBox;
};


template <typename T>
/**
 * @brief The TemplatedSpinBox class
 *
 * Maps a fundamental type to a spinbox type.
 */
struct TemplatedSpinBox;
template <>
struct TemplatedSpinBox<int>
{
  using spinbox_type = QSpinBox;
  using value_type = int;
};
template <>
struct TemplatedSpinBox<float>
{
  using spinbox_type = QDoubleSpinBox;
  using value_type = float;
};
template <>
struct TemplatedSpinBox<double>
{
  using spinbox_type = QDoubleSpinBox;
  using value_type = double;
};

/**
 * @brief The MaxRangeSpinBox class
 *
 * A spinbox mixin that will set its min and max to
 * the biggest positive and negative numbers for the given spinbox type.
 */
template <typename SpinBox>
class MaxRangeSpinBox : public SpinBox::spinbox_type
{
public:
  template <typename... Args>
  MaxRangeSpinBox(Args&&... args)
      : SpinBox::spinbox_type{std::forward<Args>(args)...}
  {
    this->setMinimum(
        std::numeric_limits<typename SpinBox::value_type>::lowest());
    this->setMaximum(std::numeric_limits<typename SpinBox::value_type>::max());
    this->setAlignment(Qt::AlignRight);
  }

  void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};

/**
 * @brief The SpinBox class
 *
 * An abstraction for the most common spinbox type in score.
 */
template <typename T>
class SpinBox final : public MaxRangeSpinBox<TemplatedSpinBox<T>>
{
public:
  using MaxRangeSpinBox<TemplatedSpinBox<T>>::MaxRangeSpinBox;
};

template <>
class SpinBox<double> final : public MaxRangeSpinBox<TemplatedSpinBox<double>>
{
public:
  template <typename... Args>
  SpinBox(Args&&... args) : MaxRangeSpinBox{std::forward<Args>(args)...}
  {
    setDecimals(5);
  }
};
template <>
class SpinBox<float> final : public MaxRangeSpinBox<TemplatedSpinBox<float>>
{
public:
  template <typename... Args>
  SpinBox(Args&&... args) : MaxRangeSpinBox{std::forward<Args>(args)...}
  {
    setDecimals(5);
  }
};

}
