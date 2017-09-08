#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

namespace Scenario
{
class TimeSyncInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("ff1d130b-caaa-4217-868b-cf09bf75823a")
public:
  TimeSyncInspectorFactory() : InspectorWidgetFactory{}
  {
  }

  QWidget* make(
      const QList<const QObject*>& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const QList<const QObject*>& objects) const override;
};
}
