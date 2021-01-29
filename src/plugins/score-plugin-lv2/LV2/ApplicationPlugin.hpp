#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <verdigris>
#include <LV2/Context.hpp>

#include <lilv/lilvmm.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QProcess>

#include <score_plugin_media_export.h>

#include <thread>

namespace LV2
{
struct HostContext;
struct GlobalContext;

class SCORE_PLUGIN_MEDIA_EXPORT ApplicationPlugin : public QObject, public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

public:
  Lilv::World lilv;
  std::unique_ptr<LV2::GlobalContext> lv2_context;
  LV2::HostContext lv2_host_context;
};

}
