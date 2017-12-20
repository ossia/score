#include "EffectFactory.hpp"
#include <Effect/EffectModel.hpp>
namespace Process
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

EffectUIFactory* EffectUIFactoryList::findDefaultFactory(
    const EffectModel& proc) const
{
  return findDefaultFactory(proc.concreteKey());
}

EffectUIFactory* EffectUIFactoryList::findDefaultFactory(
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
