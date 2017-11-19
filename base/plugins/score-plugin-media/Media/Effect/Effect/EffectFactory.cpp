#include "EffectFactory.hpp"

namespace Media
{
namespace Effect
{
EffectFactory::~EffectFactory()
{

}

EffectFactoryList::object_type*EffectFactoryList::loadMissing(
        const VisitorVariant& vis,
        QObject* parent) const
{
    SCORE_TODO;
    return nullptr;
}

}
}
