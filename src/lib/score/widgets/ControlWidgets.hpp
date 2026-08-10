#pragma once
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/IntSlider.hpp>

#include <ossia/network/value/vec.hpp>

#include <QPushButton>

#include <score_lib_base_export.h>

#include <array>

#include <verdigris>

class QDoubleSpinBox;

namespace score
{
struct SCORE_LIB_BASE_EXPORT ToggleButton : public QPushButton
{
public:
  ToggleButton(std::array<QString, 2> alts, QWidget* parent);

  ToggleButton(std::array<const char*, 2> alt, QWidget* parent);

  ToggleButton(QStringList alt, QWidget* parent);
  ~ToggleButton();

  std::array<QString, 2> alternatives;

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ValueSlider : public score::IntSlider
{
public:
  using IntSlider::IntSlider;
  ~ValueSlider();
  bool moving = false;

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT SpeedSlider : public score::DoubleSlider
{
public:
  explicit SpeedSlider(QWidget* parent = nullptr);
  ~SpeedSlider();
  bool showText = true;

  double speed() const noexcept;
  void setSpeed(double);
  void setTempo(double);

protected:
  using score::DoubleSlider::setValue;
  using score::DoubleSlider::value;

  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent*) override;
  void createPopup(QPoint pos) override;
};

struct SCORE_LIB_BASE_EXPORT VolumeSlider : public score::DoubleSlider
{
public:
  using DoubleSlider::DoubleSlider;
  double map(double v) const override; //TODO this is not very DRY friendly
  double unmap(double v) const override;
  ~VolumeSlider();

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ValueDoubleSlider : public score::DoubleSlider
{
public:
  using score::DoubleSlider::DoubleSlider;
  ~ValueDoubleSlider();

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ValueLogDoubleSlider : public score::DoubleSlider
{
public:
  using score::DoubleSlider::DoubleSlider;
  double map(double v) const override;
  double unmap(double v) const override;
  ~ValueLogDoubleSlider();

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ComboSlider : public score::IntSlider
{
  QStringList array;

public:
  template <std::size_t N>
  ComboSlider(const std::array<const char*, N>& arr, QWidget* parent)
      : score::IntSlider{parent}
  {
    array.reserve(N);
    for(auto str : arr)
      array.push_back(str);
  }

  ComboSlider(const QStringList& arr, QWidget* parent);
  ~ComboSlider();

protected:
  void paintEvent(QPaintEvent* event) override;
};

/**
 * @brief Editor for a pair (start, end), the QWidget counterpart of
 * score::QGraphicsRangeSlider.
 *
 * The API deliberately mirrors QGraphicsRangeSlider -- setRange / setValue /
 * value / moving / sliderMoved / sliderReleased -- so that the two can be
 * driven by the same code in WidgetFactory.
 *
 * The two bounds are kept ordered: pushing the start above the end drags the
 * end along, and vice-versa, so the control can never be put in a state where
 * start > end.
 */
class SCORE_LIB_BASE_EXPORT RangeWidget final : public QWidget
{
  W_OBJECT(RangeWidget)

public:
  explicit RangeWidget(bool integral, QWidget* parent = nullptr);
  ~RangeWidget();

  void setRange(double min, double max, ossia::vec2f init);
  void setValue(ossia::vec2f value);
  [[nodiscard]] ossia::vec2f value() const noexcept;

  bool moving = false;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void onStartEdited();
  void onEndEdited();

  QDoubleSpinBox* m_start{};
  QDoubleSpinBox* m_end{};
  ossia::vec2f m_init{};
  bool m_updating{};
};

SCORE_LIB_BASE_EXPORT const QPalette& transparentPalette();
}
