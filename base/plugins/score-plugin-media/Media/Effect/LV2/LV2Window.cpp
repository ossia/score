#include "LV2Window.hpp"

#include <suil-0/suil/suil.h>
#include <QtWidgets>
#include <QtGui>
#include <Media/ApplicationPlugin.hpp>
#include <score/widgets/MarginLess.hpp>
#include <ossia/network/value/value_conversion.hpp>
namespace Media::LV2
{
Window::Window(const LV2EffectModel& fx, const score::DocumentContext& ctx, QWidget* parent)
{
  if (!fx.plugin)
    throw std::runtime_error("Cannot create UI");

  auto& p = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  auto lay = new score::MarginLess<QHBoxLayout>;
  setLayout(lay);

  // Find a relevant ui
  const auto native_ui_type_uri = "http://lv2plug.in/ns/extensions/ui#Qt5UI";
  {
    auto the_uis = lilv_plugin_get_uis(fx.plugin);
    auto native_ui_type = lilv_new_uri(p.lilv.me, native_ui_type_uri);
    LILV_FOREACH(uis, u, the_uis) {
      const LilvUI* this_ui = lilv_uis_get(the_uis, u);
      if (lilv_ui_is_supported(this_ui,
                               suil_ui_supported,
                               native_ui_type,
                               &fx.effectContext.ui_type)) {
        fx.effectContext.ui = this_ui;
        break;
      }
    }
  }
  if(!fx.effectContext.ui)
    throw std::runtime_error("UI not supported");

  auto& plug = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  if(!plug.lv2_context->ui_host)
    plug.lv2_context->ui_host = suil_host_new(on_uiMessage, port_index, nullptr, nullptr);

  // Set-up features and instantiate the plug-in ui
  const LV2_Feature parent_feature = {
    LV2_UI__parent, this
  };
  const LV2_Feature instance_feature = {
    "http://lv2plug.in/ns/ext/instance-access", lilv_instance_get_handle(fx.effectContext.instance)
  };
  const LV2_Feature data_feature = {
    LV2_DATA_ACCESS_URI, &fx.effectContext.data
  };
  const LV2_Feature idle_feature = {
    LV2_UI__idleInterface, nullptr
  };
  const LV2_Feature* ui_features[] = {

    &plug.lv2_context->uri_map_feature,
    &plug.lv2_context->map_feature,
    &plug.lv2_context->unmap_feature,
    &instance_feature,
    &data_feature,
    &plug.lv2_context->logger_feature,
    &parent_feature,
    &plug.lv2_context->options_feature,
    &idle_feature,
    nullptr
  };

  const char* bundle_uri  = lilv_node_as_uri(lilv_ui_get_bundle_uri(fx.effectContext.ui));
  const char* binary_uri  = lilv_node_as_uri(lilv_ui_get_binary_uri(fx.effectContext.ui));
  char* bundle_path = lilv_file_uri_parse(bundle_uri, nullptr);
  char* binary_path = lilv_file_uri_parse(binary_uri, nullptr);

  fx.effectContext.ui_instance = suil_instance_new(
        plug.lv2_context->ui_host,
        (LV2EffectModel*) &fx,
        native_ui_type_uri,
        lilv_node_as_uri(lilv_plugin_get_uri(fx.effectContext.plugin)),
        lilv_node_as_uri(lilv_ui_get_uri(fx.effectContext.ui)),
        lilv_node_as_uri(fx.effectContext.ui_type),
        bundle_path,
        binary_path,
        ui_features);

  lilv_free(binary_path);
  lilv_free(bundle_path);

  if (!fx.effectContext.ui_instance)
    throw std::runtime_error("UI creation error");

  // Setup the widget stuff
  auto widget = (QWidget*)suil_instance_get_widget(fx.effectContext.ui_instance);
  lay->addWidget(widget);
  auto name = lilv_plugin_get_name(fx.plugin);
  setWindowTitle(lilv_node_as_string(name));
  lilv_node_free(name);
  show();

  if (!is_resizable(p.lilv.me, *fx.effectContext.ui))
  {
    widget->setMinimumSize(widget->width(), widget->height());
    widget->setMaximumSize(widget->width(), widget->height());
    adjustSize();
    setFixedSize(width(), height());
  }
  else
  {
    resize(widget->width(), widget->height());
  }

  // Set up regular updates
  connect(&ctx.coarseUpdateTimer, &QTimer::timeout,
          this, [&] {
    // score -> UI
    {
      Message ev;
      while(fx.plugin_events.try_dequeue(ev))
      {
        suil_instance_port_event(fx.effectContext.ui_instance, ev.index,
                                 ev.body.size(), ev.protocol, ev.body.data());
      }
    }

    // UI -> score
    {
      auto& plug = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
      Message ev;
      while(fx.ui_events.try_dequeue(ev))
      {
        if (ev.protocol == 0)
        {
          SCORE_ASSERT(ev.body.size() == sizeof(float));

          auto port = fx.control_map.at(ev.index).first;
          SCORE_ASSERT(port);

          float f = *(float*)ev.body.data();
          port->setValue(f);
        }
        else if (ev.protocol == plug.lv2_host_context.atom_eventTransfer)
        {
          qDebug() << "LV2: EventTransfer";
          /* TODO
        LV2_Evbuf_Iterator    e    = lv2_evbuf_end(port->evbuf);
        const LV2_Atom* const atom = (const LV2_Atom*)body;
        lv2_evbuf_write(&e, nframes, 0, atom->type, atom->size,
                        (const uint8_t*)LV2_ATOM_BODY_CONST(atom));
                        */
        }
        else
        {
          qDebug() << "LV2: Unknown protocol" << ev.protocol;
        }
      }
    }
  });

  // Set initial control port values
  for(auto& e : fx.control_map)
  {
    float f = ossia::convert<float>(e.second.first->value());
    suil_instance_port_event(fx.effectContext.ui_instance, e.first, sizeof(float), 0, &f);
  }
}

Window::~Window()
{

}

bool Window::is_resizable(LilvWorld* world, const LilvUI& ui)
{
  auto& plug = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  auto& h = plug.lv2_host_context;
  auto s = lilv_ui_get_uri(&ui);

  Lilv::Nodes fs_matches = plug.lilv.find_nodes(s, h.optional_feature, h.fixed_size);
  Lilv::Nodes nrs_matches = plug.lilv.find_nodes(s, h.optional_feature, h.fixed_size);

  return fs_matches.me == nullptr && nrs_matches.me == nullptr;
}

void Window::on_uiMessage(
    SuilController controller,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t protocol,
    const void* buffer)
{
  LV2EffectModel& jalv = *(LV2EffectModel*)controller;

  auto it = jalv.control_map.find(port_index);
  if (it == jalv.control_map.end()) {
    qDebug() << jalv.effect() << " (LV2): invalid write on port" << port_index;
    return;
  }

  // currently writing from score
  if(it->second.second)
    return;

  Message c{port_index, protocol, {}};
  c.body.resize(buffer_size);
  auto b = (const uint8_t*) buffer;
  for(uint32_t i = 0; i < buffer_size; i++)
    c.body[i] = b[i];

  jalv.ui_events.enqueue(std::move(c));
}

uint32_t Window::port_index(SuilController controller, const char* symbol)
{
  auto& p = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  LV2EffectModel& jalv = (LV2EffectModel&)controller;
  auto n = lilv_new_uri(p.lilv.me, symbol);
  auto port = lilv_plugin_get_port_by_symbol(jalv.plugin, n);
  lilv_node_free(n);
  return port ? lilv_port_get_index(jalv.plugin, port) : LV2UI_INVALID_PORT_INDEX;
}


}
