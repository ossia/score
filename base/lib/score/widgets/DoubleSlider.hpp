#pragma once
#include <QSlider>
#include <score/tools/Clamp.hpp>
#include <QDebug>
#include <limits>

#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief The DoubleSlider class
 *
 * Always between 0. - 1.
 */
class SCORE_LIB_BASE_EXPORT DoubleSlider : public QSlider
{
  Q_OBJECT

public:
  static const constexpr double max = std::numeric_limits<int>::max() / 65536.;
  DoubleSlider(QWidget* parent) : QSlider{Qt::Horizontal, parent}
  {
    setMinimum(0);
    setMaximum(max+1);

    connect(this, &QSlider::valueChanged, this, [&](int val) {
      emit valueChanged(double(val) / max);
    });
  }

  virtual ~DoubleSlider();

  void setValue(double val)
  {
    val = clamp(val, 0, 1);
    blockSignals(true);
    QSlider::setValue(val * max);
    blockSignals(false);
  }

  double value() const
  {
    return QSlider::value() / max;
  }

Q_SIGNALS:
  void valueChanged(double);
};
}
