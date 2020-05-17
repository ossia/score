#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <QList>

namespace Scenario
{
class ScenarioInspectorWidgetFactoryWrapper final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("066fffc1-c82c-4ffd-ad7c-55a65bfa067f")
public:
  ScenarioInspectorWidgetFactoryWrapper() : InspectorWidgetFactory{} { }

  ~ScenarioInspectorWidgetFactoryWrapper() override;

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool update(QWidget* cur, const QList<const IdentifiedObjectAbstract*>& obj) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}
