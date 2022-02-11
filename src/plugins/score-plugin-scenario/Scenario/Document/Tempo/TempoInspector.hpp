#pragma once
#include <Scenario/Document/Tempo/TempoProcess.hpp>
#include <Automation/Inspector/CurvePointInspectorWidget.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace Scenario
{

class TempoPointInspectorWidget final
    : public Curve::PointInspectorWidget
{
public:
  explicit TempoPointInspectorWidget(
      const Curve::PointModel& model,
      const score::DocumentContext& doc,
      QWidget* parent);

private:
  void on_pointChanged(double);
  void on_editFinished();

  OngoingCommandDispatcher& m_dispatcher;

  QDoubleSpinBox* m_YBox{};
  double m_yFactor{};
  double m_Ymin{};
};

class TempoPointInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("c632cc83-d82b-45d8-9495-830b0e7830aa")
public:
  TempoPointInspectorFactory();

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}
