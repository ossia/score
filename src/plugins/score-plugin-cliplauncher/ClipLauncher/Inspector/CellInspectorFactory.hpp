#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace ClipLauncher
{

class CellInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("a1b2c3d4-e5f6-7890-abcd-ef1234567890")
public:
  CellInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements, const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};

} // namespace ClipLauncher
