#include <LV2/Window.hpp>

#include <LV2/ApplicationPlugin.hpp>
#include <Media/Effect/Settings/Model.hpp>

#include <score/widgets/MarginLess.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QHBoxLayout>
#include <QTimer>

#include <wobjectimpl.h>

#include <suil-0/suil/suil.h>

W_OBJECT_IMPL(LV2::Window)
namespace LV2
{
Window::Window(const Model& fx, const score::DocumentContext& ctx, QWidget* parent)
    : m_model{fx}
{
  if (!fx.plugin)
    throw std::runtime_error("Cannot create UI");

  bool ontop = ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop();
  if (ontop)
  {
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
  }
  setAttribute(Qt::WA_DeleteOnClose, true);

  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  auto lay = new score::MarginLess<QHBoxLayout>;
  setLayout(lay);

  // Find a relevant ui
  const auto native_ui_type_uri = "http://lv2plug.in/ns/extensions/ui#Qt5UI";
  {
    auto the_uis = lilv_plugin_get_uis(fx.plugin);
    auto native_ui_type = lilv_new_uri(p.lilv.me, native_ui_type_uri);
    LILV_FOREACH(uis, u, the_uis)
    {
      const LilvUI* this_ui = lilv_uis_get(the_uis, u);
      if (lilv_ui_is_supported(
              this_ui, suil_ui_supported, native_ui_type, &fx.effectContext.ui_type))
      {
        fx.effectContext.ui = this_ui;
        break;
      }
    }
  }
  if (!fx.effectContext.ui)
    throw std::runtime_error("UI not supported");

  auto& plug = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  // Set-up features and instantiate the plug-in ui
  const LV2_Feature parent_feature = {LV2_UI__parent, this};
  const LV2_Feature instance_feature
      = {"http://lv2plug.in/ns/ext/instance-access",
         lilv_instance_get_handle(fx.effectContext.instance)};
  const LV2_Feature data_feature = {LV2_DATA_ACCESS_URI, &fx.effectContext.data};
  const LV2_Feature idle_feature = {LV2_UI__idleInterface, nullptr};
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
      nullptr};

  const char* bundle_uri = lilv_node_as_uri(lilv_ui_get_bundle_uri(fx.effectContext.ui));
  const char* binary_uri = lilv_node_as_uri(lilv_ui_get_binary_uri(fx.effectContext.ui));
  char* bundle_path = lilv_file_uri_parse(bundle_uri, nullptr);
  char* binary_path = lilv_file_uri_parse(binary_uri, nullptr);

  fx.effectContext.ui_instance = suil_instance_new(
      plug.lv2_context->ui_host,
      (Model*)&fx,
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

  const int default_w = widget->width();
  const int default_h = widget->height();

  lay->addWidget(widget);
  m_widget = widget;
  auto name = lilv_plugin_get_name(fx.plugin);
  setWindowTitle(lilv_node_as_string(name));
  lilv_node_free(name);

  // Set up regular updates
  QPointer<const Model> fx_ptr{&fx};
  connect(&ctx.coarseUpdateTimer, &QTimer::timeout, this, [&, fx_ptr] {
    // score -> UI
    if (!fx_ptr)
      return;

    {
      Message ev;
      while (fx.plugin_events.try_dequeue(ev))
      {
        suil_instance_port_event(
            fx.effectContext.ui_instance, ev.index, ev.body.size(), ev.protocol, ev.body.data());
      }
    }

    // UI -> score
    {
      auto& plug = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
      Message ev;
      while (fx.ui_events.try_dequeue(ev))
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
  for (auto& e : fx.control_map)
  {
    float f = ossia::convert<float>(e.second.first->value());
    suil_instance_port_event(fx.effectContext.ui_instance, e.first, sizeof(float), 0, &f);
  }

  // Show ui and resize
  QTimer::singleShot(0, [=, &p] {
    if (!is_resizable(p.lilv.me, *m_model.effectContext.ui))
    {
      widget->setMinimumSize(default_w, default_h);
      widget->setMaximumSize(default_w, default_h);
      adjustSize();
      setFixedSize(width(), height());
    }
    else
    {
      using namespace std;
      resize(min(800, default_w), min(800, default_h));
    }
  });

  fx.externalUIVisible(true);
}

Window::~Window() { }

void Window::resizeEvent(QResizeEvent* event)
{
  QDialog::resizeEvent(event);
  /*
  auto& p =
  score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>(); if
  (is_resizable(p.lilv.me, *effect.effectContext.ui))
  {
    resize(m_widget->width(), m_widget->height());
  }
  */
}

void Window::closeEvent(QCloseEvent* event)
{
  if (m_widget)
    m_widget->setParent(nullptr);
  suil_instance_free(m_model.effectContext.ui_instance);
  m_model.effectContext.ui_instance = nullptr;
  m_model.externalUIVisible(false);
  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  QDialog::closeEvent(event);
}

bool Window::is_resizable(LilvWorld* world, const LilvUI& ui)
{
  auto& plug = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  auto& h = plug.lv2_host_context;
  auto s = lilv_ui_get_uri(&ui);

  Lilv::Nodes fs_matches = plug.lilv.find_nodes(s, h.optional_feature, h.fixed_size);
  Lilv::Nodes nrs_matches = plug.lilv.find_nodes(s, h.optional_feature, h.no_user_resize);

  return fs_matches.me == nullptr && nrs_matches.me == nullptr;
}
}
