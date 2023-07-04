
#include <avnd/../../examples/Advanced/Utilities/AudioFilters.hpp>

#include <Avnd/Factories.hpp>

#include <Aether/src/aether_dsp.cpp>

namespace oscr
{
void instantiate_aether(
    std::vector<score::InterfaceBase*>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  oscr::instantiate_fx<Aether::Object>(fx, ctx, key);
}
}
