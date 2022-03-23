#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

#include <Curve/Palette/CommandObjects/MovePointCommandObject.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score_plugin_automation_export.h>

class QDoubleSpinBox;
class QWidget;
namespace Inspector {
class Layout;
}
namespace Curve
{
class PointModel;
class SCORE_PLUGIN_AUTOMATION_EXPORT PointInspectorWidget
    : public Inspector::InspectorWidgetBase
{
public:
  explicit PointInspectorWidget(
      const Curve::PointModel& model,
      const score::DocumentContext& context,
      QWidget* parent);

protected:
  QPointer<const Curve::PointModel> m_model;

  Inspector::Layout* m_layout{};
  QDoubleSpinBox* m_XBox{};
  double m_xFactor{};
};
}

namespace Automation
{
class PointInspectorWidget final : public Curve::PointInspectorWidget
{
public:
  explicit PointInspectorWidget(
      const Curve::PointModel& model,
      const score::DocumentContext& context,
      QWidget* parent);

private:
  void on_pointXChanged(double);
  void on_pointYChanged(double);
  void on_editXFinished();
  void on_editYFinished();
  Curve::StateBase* m_moveState{};
  Curve::MovePointCommandObject m_moveX;
  OngoingCommandDispatcher& m_dispatcher;

  QDoubleSpinBox* m_YBox{};
  double m_yFactor{};
  double m_Ymin{};
  bool m_startedEditing{};
};
}
