#include "ApplicationPlugin.hpp"

#if defined(LILV_SHARED)
#include <Media/Effect/LV2/LV2Context.hpp>
#endif

namespace Media
{

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& app):
    score::ApplicationPlugin{app}
  #if defined(LILV_SHARED)
  , lv2_context{std::make_unique<LV2::GlobalContext>(64, lv2_host_context)}
  , lv2_host_context{lv2_context.get(), nullptr, lv2_context->features(), lilv}
  #endif
{
#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
    lv2_context->loadPlugins();
#endif
}

ApplicationPlugin::~ApplicationPlugin()
{

}
}
