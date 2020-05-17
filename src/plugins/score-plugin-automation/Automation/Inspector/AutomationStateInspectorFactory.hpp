#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace Automation
{
class StateInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("71a5f5b6-6c10-4057-ab10-278c3f18e9af")
public:
  StateInspectorFactory();

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}
