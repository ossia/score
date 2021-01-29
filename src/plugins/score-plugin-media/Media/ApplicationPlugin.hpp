#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <verdigris>
#if defined(HAS_LV2)
#include <Media/Effect/LV2/LV2Context.hpp>

#include <lilv/lilvmm.hpp>
#endif

#include <ossia/detail/hash_map.hpp>

#include <QProcess>

#include <score_plugin_media_export.h>

#include <thread>
namespace Media
{
namespace LV2
{
struct HostContext;
struct GlobalContext;
}
class SCORE_PLUGIN_MEDIA_EXPORT ApplicationPlugin : public QObject, public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

#if defined(HAS_LV2) // TODO instead add a proper preprocessor macro that
                     // also works in static case
public:
  Lilv::World lilv;
  std::unique_ptr<LV2::GlobalContext> lv2_context;
  LV2::HostContext lv2_host_context;
#endif

};

class GUIApplicationPlugin : public QObject, public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  void initialize() override;
};
}
