#pragma once
#include <ysfx.h>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/hash_map.hpp>

#include <verdigris>

namespace YSFX
{
struct HostContext;
struct GlobalContext;

class ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
public:
  ApplicationPlugin(const score::ApplicationContext& app)
      : score::ApplicationPlugin{app}
  {
    ysfx_register_builtin_audio_formats(config.get());
  }

  ~ApplicationPlugin() override { }

  ysfx_config_u config{ysfx_config_new()};
};

}
