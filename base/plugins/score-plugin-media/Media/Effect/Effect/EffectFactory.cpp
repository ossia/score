#include "EffectFactory.hpp"
#include <Media/Effect/Effect/EffectModel.hpp>
namespace Media
{
namespace Effect
{
EffectFactory::~EffectFactory()
{

}
EffectUIFactory::~EffectUIFactory()
{

}

EffectFactoryList::object_type*EffectFactoryList::loadMissing(
        const VisitorVariant& vis,
        QObject* parent) const
{
    SCORE_TODO;
    return nullptr;
}

bool EffectUIFactory::matches(const EffectModel& p) const
{
  return matches(p.concreteKey());
}

EffectUIFactory*EffectUIFactoryList::findDefaultFactory(
    const EffectModel& proc) const
{
  return findDefaultFactory(proc.concreteKey());
}

EffectUIFactory*Media::Effect::EffectUIFactoryList::findDefaultFactory(
    const UuidKey<EffectModel>& proc) const
{
  for (auto& fac : *this)
  {
    if (fac.matches(proc))
      return &fac;
  }
  return nullptr;
}

}
}
