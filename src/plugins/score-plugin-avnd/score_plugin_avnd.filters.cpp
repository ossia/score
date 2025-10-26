
#include <avnd/../../examples/Advanced/Audio/AudioFilters.hpp>

#include <Avnd/Factories.hpp>
namespace oscr
{
void instantiate_audiofilters(
    std::vector<score::InterfaceBase*>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  oscr::instantiate_fx<ao::Lowpass>(fx, ctx, key);
  oscr::instantiate_fx<ao::Highpass>(fx, ctx, key);
  oscr::instantiate_fx<ao::Lowshelf>(fx, ctx, key);
  oscr::instantiate_fx<ao::Highshelf>(fx, ctx, key);
  oscr::instantiate_fx<ao::Bandpass>(fx, ctx, key);
  oscr::instantiate_fx<ao::Bandstop>(fx, ctx, key);
  oscr::instantiate_fx<ao::Bandshelf>(fx, ctx, key);
}
}
