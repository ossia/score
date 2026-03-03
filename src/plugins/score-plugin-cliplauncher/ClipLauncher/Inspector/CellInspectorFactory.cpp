#include "CellInspectorFactory.hpp"

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/Inspector/CellInspectorWidget.hpp>

namespace ClipLauncher
{

QWidget* CellInspectorFactory::make(
    const InspectedObjects& sourceElements, const score::DocumentContext& doc,
    QWidget* parent) const
{
  return new CellInspectorWidget{
      safe_cast<const CellModel&>(*sourceElements.first()), doc, parent};
}

bool CellInspectorFactory::matches(const InspectedObjects& objects) const
{
  return dynamic_cast<const CellModel*>(objects.first());
}

} // namespace ClipLauncher
