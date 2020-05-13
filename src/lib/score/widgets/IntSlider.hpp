#pragma once
#include <QProxyStyle>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT IntSlider: public QWidget
{
    W_OBJECT(IntSlider)
public:
  IntSlider(Qt::Orientation ort, QWidget* widg);
  IntSlider(QWidget* widg);
  ~IntSlider() override;

  void setValue(int val);
  void setMinimum(int min) {m_min = min;}
  void setMaximum(int max) {m_max = max;}
  void setRange(int min, double max){m_min = min; m_max = max;}
  void setOrientation(Qt::Orientation ort) {m_orientation = ort;}

  int value() const {return m_value;}
  int minimum() const {return m_min;}
  int maximum() const {return m_max;}

  void valueChanged(int arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved(int arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved, arg_1)
  void sliderReleased()
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

protected:
  void paintEvent(QPaintEvent*) override;
  void paint(QPainter& p);
  void paintWithText(const QString& s);

private:
  void updateValue(QPointF mousePos);

  int m_value;
  int m_min;
  int m_max;

  Qt::Orientation m_orientation;
  double m_borderWidth;
};
}
