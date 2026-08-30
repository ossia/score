#include "SceneInspectorFactory.hpp"

#include <ClipLauncher/Inspector/SceneInspectorWidget.hpp>
#include <ClipLauncher/SceneModel.hpp>

namespace ClipLauncher
{

QWidget* SceneInspectorFactory::make(
    const InspectedObjects& sourceElements, const score::DocumentContext& doc,
    QWidget* parent) const
{
  return new SceneInspectorWidget{
      safe_cast<const SceneModel&>(*sourceElements.first()), doc, parent};
}

bool SceneInspectorFactory::matches(const InspectedObjects& objects) const
{
  return dynamic_cast<const SceneModel*>(objects.first());
}

} // namespace ClipLauncher
