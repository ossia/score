#pragma once
#include <QWidget>
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

  enum TimeMode
  {
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
  void initStyleOption(QStyleOptionFrame* option) const noexcept;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

private:
  void updateTime();
  QPoint m_startPos{};
  int64_t m_prevY{};
  int64_t m_travelledY{};

  int64_t m_origFlicks{};
  int64_t m_flicks{};

  ossia::bar_time m_barTime{};

  enum GrabbedHandle
  {
    None,
    Bar,
    Quarter,
    Semiquaver,
    Cent
  } m_grab{None};

  TimeMode m_mode{Bars};

  friend struct BarSpinBox;
  friend struct SecondSpinBox;
  friend struct FlicksSpinBox;
};
}
