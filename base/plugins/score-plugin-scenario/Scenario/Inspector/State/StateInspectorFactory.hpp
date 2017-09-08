#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

namespace Scenario
{
class StateInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("780a33ea-408a-4719-b4cc-52a2d8922478")
public:
  StateInspectorFactory() : InspectorWidgetFactory{}
  {
  }

  QWidget* make(
      const QList<const QObject*>& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const QList<const QObject*>& objects) const override;
};
}
