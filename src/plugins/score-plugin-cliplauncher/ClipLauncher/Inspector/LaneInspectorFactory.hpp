#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace ClipLauncher
{

class LaneInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("b2c3d4e5-f6a7-8901-bcde-f23456789012")
public:
  LaneInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements, const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};

} // namespace ClipLauncher
