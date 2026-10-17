#include "PatternFactory.hpp"

#include <Patternist/PatternEditor.hpp>

namespace Patternist
{
bool LayerFactory::hasExternalUI(
    const Process::ProcessModel&, const score::DocumentContext&) const noexcept
{
  return true;
}

QWidget* LayerFactory::makeExternalUI(
    Process::ProcessModel& proc, const score::DocumentContext& ctx,
    QWidget* parent) const
{
  return new Editor{static_cast<ProcessModel&>(proc), ctx, parent};
}
}
