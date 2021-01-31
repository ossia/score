#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <verdigris>
#include <LV2/Context.hpp>

#include <lilv/lilvmm.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QProcess>

#include <thread>

namespace LV2
{
struct HostContext;
struct GlobalContext;

class ApplicationPlugin : public QObject, public score::ApplicationPlugin
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
