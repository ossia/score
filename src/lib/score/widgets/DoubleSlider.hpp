#pragma once
#include <QProxyStyle>
#include <QSlider>

#include <score_lib_base_export.h>

#include <limits>
#include <verdigris>

namespace score
{
/**
- * @brief The DoubleSlider class
- *
- * Always between 0. - 1.
- */
class SCORE_LIB_BASE_EXPORT DoubleSlider : public QWidget //: public QSlider
{
  W_OBJECT(DoubleSlider)
public:
  DoubleSlider(Qt::Orientation ort, QWidget* widg);
  DoubleSlider(QWidget* widg);
  ~DoubleSlider() override;

  void setValue(double val);
  void setOrientation(Qt::Orientation ort) { m_orientation = ort; }
  void setBorderWidth(double border) { m_borderWidth = border; }

  double value() const { return m_value; }

  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved, arg_1)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

protected:
  void paintEvent(QPaintEvent*) override;
  void paint(QPainter& p);
  void paintWithText(const QString& s);

private:
  void updateValue(QPointF mousePos);

  double m_value;

  Qt::Orientation m_orientation;

  double m_borderWidth;
};
}
