#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <unordered_map>
#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
#include <lilv/lilvmm.hpp>
#include <Media/Effect/LV2/LV2Context.hpp>
#endif

#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTLoader.hpp>
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
        void initialize() override;
        ~ApplicationPlugin() override;

#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
    public:
        Lilv::World lilv;
        std::unique_ptr<LV2::GlobalContext> lv2_context;
        LV2::HostContext lv2_host_context;
#endif

#if defined(HAS_VST2)
  public:
        void rescanVSTs(const QStringList& );
        struct vst_info
        {
            QString path;
            QString prettyName;
            int32_t uniqueID{};
            bool isSynth{};
        };
        std::vector<vst_info> vst_infos;
        std::unordered_map<int32_t, Media::VST::VSTModule*> vst_modules;
#endif
};

}
