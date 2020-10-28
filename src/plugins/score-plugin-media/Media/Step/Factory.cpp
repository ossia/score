#include <Media/Step/Factory.hpp>
#include <Media/Step/View.hpp>
namespace Media::Step
{
score::ResizeableItem* LayerFactory::makeItem(
    const Process::ProcessModel& m, const Process::Context& ctx, QGraphicsItem* parent) const
{
  return new Item{static_cast<const Model&>(m), ctx, parent};
}
}
