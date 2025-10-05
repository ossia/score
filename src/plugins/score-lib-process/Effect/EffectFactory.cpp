#include "EffectFactory.hpp"

#include <Control/DefaultEffectItem.hpp>

namespace Process
{

EffectLayerFactory_Base::EffectLayerFactory_Base() = default;
EffectLayerFactory_Base::~EffectLayerFactory_Base() = default;

LayerView* EffectLayerFactory_Base::makeLayerView(
    const Process::ProcessModel&, const Process::Context& context,
    QGraphicsItem* parent) const
{
  return new EffectLayerView{parent};
}

LayerPresenter* EffectLayerFactory_Base::makeLayerPresenter(
    const Process::ProcessModel& lm, Process::LayerView* v,
    const Process::Context& context, QObject* parent) const
{
  auto pres
      = new EffectLayerPresenter{lm, safe_cast<EffectLayerView*>(v), context, parent};

  auto rect = new score::EmptyRectItem{v};
  auto item = makeItem(lm, context, rect);
  item->setParentItem(rect);
  return pres;
}

score::ResizeableItem* EffectLayerFactory_Base::makeItem(
    const Process::ProcessModel& proc, const Process::Context& ctx,
    QGraphicsItem* parent) const
{
  return new Process::DefaultEffectItem{false, proc, ctx, parent};
}
}
