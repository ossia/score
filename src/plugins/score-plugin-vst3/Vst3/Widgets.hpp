#pragma once
#include <Process/Style/ScenarioStyle.hpp>

#include <Vst3/EffectModel.hpp>

#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/TextItem.hpp>

#include <ossia/dataflow/safe_nodes/port.hpp>

#include <QDialog>

#include <verdigris>

namespace vst3
{

class VSTGraphicsSlider final
    : public QObject
    , public score::QGraphicsSliderBase<VSTGraphicsSlider>
{
  W_OBJECT(VSTGraphicsSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct QGraphicsSliderBase<VSTGraphicsSlider>;

  double m_value{};
  double m_execValue{};
  Steinberg::Vst::IEditController* fx{};
  Steinberg::Vst::ParamID num{};

private:
  bool m_grab{};
  bool m_hasExec{};

public:
  static const constexpr double min = 0.;
  static const constexpr double max = 1.;
  double init = 0.5;
  friend struct score::DefaultControlImpl;
  friend struct score::DefaultGraphicsSliderImpl;
  VSTGraphicsSlider(
      Steinberg::Vst::IEditController* fx, Steinberg::Vst::ParamID num,
      QGraphicsItem* parent);

  static double from01(double v) { return v; }
  static double map(double v) { return v; }
  static double unmap(double v) { return v; }

  void setValue(double v);
  double value() const;
  void setExecutionValue(const ossia::value& v);
  void resetExecution();

  bool moving = false;

public:
  void valueChanged(double arg_1) W_SIGNAL(valueChanged, arg_1);
  void sliderMoved() W_SIGNAL(sliderMoved);
  void sliderReleased() W_SIGNAL(sliderReleased);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};

struct VSTFloatSlider
{
  const std::string_view name;

  explicit constexpr VSTFloatSlider(const char* name)
      : name{name}
  {
  }

  static QWidget* make_widget(
      Steinberg::Vst::IEditController* fx, const ControlInlet& inlet,
      const score::DocumentContext& ctx, QWidget* parent, QObject* context);
  static QGraphicsItem* make_item(
      Steinberg::Vst::IEditController* fx, ControlInlet& inlet,
      const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
};

}
