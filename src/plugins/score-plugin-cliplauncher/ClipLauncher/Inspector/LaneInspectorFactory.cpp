#include "LaneInspectorFactory.hpp"

#include <ClipLauncher/Inspector/LaneInspectorWidget.hpp>
#include <ClipLauncher/LaneModel.hpp>

namespace ClipLauncher
{

QWidget* LaneInspectorFactory::make(
    const InspectedObjects& sourceElements, const score::DocumentContext& doc,
    QWidget* parent) const
{
  return new LaneInspectorWidget{
      safe_cast<const LaneModel&>(*sourceElements.first()), doc, parent};
}

bool LaneInspectorFactory::matches(const InspectedObjects& objects) const
{
  return dynamic_cast<const LaneModel*>(objects.first());
}

} // namespace ClipLauncher
