#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace ClipLauncher
{

class SceneInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("c3d4e5f6-a7b8-9012-cdef-345678901234")
public:
  SceneInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements, const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};

} // namespace ClipLauncher
