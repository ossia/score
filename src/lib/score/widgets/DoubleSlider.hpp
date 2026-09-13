#pragma once
#include <QProxyStyle>
#include <QSize>
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
- * min and max properties are for value display purpose only
- */
class SCORE_LIB_BASE_EXPORT DoubleSlider : public QWidget //: public QSlider
{
  W_OBJECT(DoubleSlider)
public:
  DoubleSlider(Qt::Orientation ort, QWidget* widg);
  DoubleSlider(QWidget* widg);
  ~DoubleSlider() override;
  bool moving = false;
  void setValue(double val);
  void setOrientation(Qt::Orientation ort)
  {
    m_orientation = ort;
    updateSkinMetrics();
  }
  void setBorderWidth(double border)
  {
    m_borderWidth = border;
    m_borderOverridden = true;
  }

  double value() const { return m_value; }
  virtual double map(double v) const;
  virtual double unmap(double v) const;
  double min{}; //TODO make it private. Warning used like this in a lot of places.
  double max{}; //ditto
  double init{}; //ditto

  void valueChanged(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueChanged, arg_1)
  void sliderMoved(double arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved, arg_1)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  virtual void createPopup(QPoint pos);
  virtual void setRange(double min, double max, double init) noexcept;

  //! Thickness of one line of the slider font: the height of a horizontal
  //! slider, the width of a vertical one. A caller pinning the control with
  //! setFixedSize should size it from here.
  int skinExtent() const noexcept;

protected:
  //! Sizes the control from one line of the slider font.
  void updateSkinMetrics();

  void paintEvent(QPaintEvent*) override;
  void paint(QPainter& p);
  void paintWithText(const QString& s);

private:
  void updateValue(QPointF mousePos);

  double m_value{};
  Qt::Orientation m_orientation{};
  double m_borderWidth{};
  QSize m_skinMinimum;
  bool m_borderOverridden{};
};
}
