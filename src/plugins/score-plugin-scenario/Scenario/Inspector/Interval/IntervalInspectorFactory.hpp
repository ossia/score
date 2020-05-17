#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

class QObject;
class QString;
class QWidget;
namespace Scenario
{
class IntervalInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("1ca16c0a-6c01-4054-a646-d860a3886e81")
public:
  IntervalInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}
