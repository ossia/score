#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>


#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
#include <lilv/lilvmm.hpp>
#include <Media/Effect/LV2/LV2Context.hpp>
#endif
namespace Media
{
namespace LV2
{
struct HostContext;
struct GlobalContext;
}
class ApplicationPlugin : public QObject, public score::ApplicationPlugin
{
        Q_OBJECT
    public:
        ApplicationPlugin(const score::ApplicationContext& app);
        ~ApplicationPlugin();

#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
    public:
        Lilv::World lilv;
        std::unique_ptr<LV2::GlobalContext> lv2_context;
        LV2::HostContext lv2_host_context;
#endif

};

}
