
#include <Avnd/Factories.hpp>

#include <avnd/../../examples/Raw/SpanControls.hpp>
namespace oscr
{
static_assert(avnd::span_value<std::span<float>>);
template <std::size_t extent>
ossia::value to_ossia_value(const std::span<float, extent>& obj)
    //{
    //  return to_ossia_value_rec(obj);
    //}
    //
    = delete;
void instantiate_tests2(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  oscr::instantiate_fx<examples::SpanControls>(fx, ctx, key);
}
}
