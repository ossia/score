#pragma once
#include <QProxyStyle>
#include <QSlider>

#include <score_lib_base_export.h>

#include <limits>
#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT AbsoluteSliderStyle final : public QProxyStyle
{
public:
  using QProxyStyle::QProxyStyle;
  ~AbsoluteSliderStyle() override;

  static AbsoluteSliderStyle* instance() noexcept;

  int styleHint(
      QStyle::StyleHint hint,
      const QStyleOption* option,
      const QWidget* widget,
      QStyleHintReturn* returnData) const override;
};

class SCORE_LIB_BASE_EXPORT Slider : public QSlider
{
public:
  Slider(Qt::Orientation ort, QWidget* widg);
  Slider(QWidget* widg);
  ~Slider() override;

protected:
  void paintEvent(QPaintEvent*) override;
  void paint(QPainter& p);
  void paintWithText(const QString& s);
};

/**
 * @brief The DoubleSlider class
 *
 * Always between 0. - 1.
 */
class SCORE_LIB_BASE_EXPORT DoubleSlider : public Slider
{
  W_OBJECT(DoubleSlider)

public:
  static const constexpr double max = std::numeric_limits<int>::max() / 65536.;
  DoubleSlider(QWidget* parent);

  ~DoubleSlider() override;
  void setValue(double val);
  double value() const;

public:
  void doubleValueChanged(double arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, doubleValueChanged, arg_1)
private:
    using QSlider::valueChanged;
};
}
