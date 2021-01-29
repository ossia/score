#include "ApplicationPlugin.hpp"

#include <Media/Effect/Settings/Model.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <Audio/AudioDevice.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(LV2::ApplicationPlugin)


namespace LV2
{
void on_uiMessage(
    SuilController controller,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t protocol,
    const void* buffer)
{
  auto& fx = *(Model*)controller;

  auto it = fx.control_map.find(port_index);
  if (it == fx.control_map.end())
  {
    qDebug() << fx.effect() << " (LV2): invalid write on port" << port_index;
    return;
  }

  // currently writing from score
  if (it->second.second)
    return;

  Message c{port_index, protocol, {}};
  c.body.resize(buffer_size);
  auto b = (const uint8_t*)buffer;
  for (uint32_t i = 0; i < buffer_size; i++)
    c.body[i] = b[i];

  fx.ui_events.enqueue(std::move(c));
}

uint32_t port_index(SuilController controller, const char* symbol)
{
  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  Model& fx = (Model&)controller;
  auto n = lilv_new_uri(p.lilv.me, symbol);
  auto port = lilv_plugin_get_port_by_symbol(fx.plugin, n);
  lilv_node_free(n);
  return port ? lilv_port_get_index(fx.plugin, port) : LV2UI_INVALID_PORT_INDEX;
}
}

namespace LV2
{

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& app)
    : score::ApplicationPlugin{app}
    , lv2_context{std::make_unique<LV2::GlobalContext>(64, lv2_host_context)}
    , lv2_host_context{lv2_context.get(), nullptr, lv2_context->features(), lilv}
{
  static int argc{0};
  static char** argv{nullptr};
  suil_init(&argc, &argv, SUIL_ARG_NONE);
  QString res = qgetenv("SCORE_DISABLE_LV2");
  if (res.isEmpty())
    lv2_context->loadPlugins();

  lv2_context->ui_host
      = suil_host_new(LV2::on_uiMessage, LV2::port_index, nullptr, nullptr);
}

void ApplicationPlugin::initialize()
{
}


ApplicationPlugin::~ApplicationPlugin()
{
  suil_host_free(lv2_context->ui_host);
}

}
