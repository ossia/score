#include <Process/Dataflow/Port.hpp>

#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Window.hpp>
#include <Media/Effect/Settings/Model.hpp>

#include <score/tools/ElfInspector.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QTimer>

#include <wobjectimpl.h>

#include <suil-0/suil/suil.h>

#include <array>

W_OBJECT_IMPL(LV2::Window)
namespace LV2
{
//! An X11UI binary linked against another Qt major version cannot run in
//! this process: Qt keeps the same mangled names across majors for the
//! exported QString/QByteArray operators, so the other Qt's internal calls
//! get resolved to *our* Qt's code operating on their objects - memory
//! corruption before the UI even finishes creating its QApplication
//! (observed with qmidiarp's Qt5 UI: Qt5's QFactoryLoader ends up in Qt6's
//! operator<(QString, QString) and dies). GTK-based hosts load these UIs
//! fine since no other Qt lives in their process; we must refuse.
//!
//! On Linux this reads the actual DT_NEEDED entries; elsewhere (and for
//! binaries the ELF reader cannot parse) it falls back to scanning for the
//! dependency name patterns of every platform's Qt linkage.
bool uiLinksIncompatibleQt(const QString& binary_path)
{
#if defined(__linux__)
  if(const auto deps
     = score::ElfInspector{}.try_get_dt_needed(binary_path.toStdString()))
  {
    for(const std::string& dep : *deps)
    {
      // libQt5Core.so.5, libQt5Gui.so.5, libQt5Widgets.so.5, ...
      constexpr std::string_view prefix = "libQt";
      if(dep.size() > prefix.size() && dep.starts_with(prefix))
      {
        const char c = dep[prefix.size()];
        if(c >= '0' && c <= '9' && (c - '0') != QT_VERSION_MAJOR)
          return true;
      }
    }
    return false; // definitive answer
  }
#endif

  QFile f{binary_path};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  // Dependency names appear verbatim in the binary (ELF .dynstr, Mach-O
  // load commands, PE import table); a substring scan is enough
  const QByteArray data = f.readAll();
  static constexpr std::array others{4, 5, 6, 7};
  for(int major : others)
  {
    if(major == QT_VERSION_MAJOR)
      continue;
    const QByteArray n = QByteArray::number(major);
    const QByteArray patterns[]{
        "libQt" + n + "Core",                 // Linux .so / macOS .dylib
        "libQt" + n + "Gui",
        "Qt" + n + "Core.dll",                // Windows
        "Qt" + n + "Gui.dll",
        "QtCore.framework/Versions/" + n,     // macOS frameworks (Qt4/Qt5
        "QtGui.framework/Versions/" + n};     // used the major as version)
    for(const auto& pattern : patterns)
      if(data.contains(pattern))
        return true;
  }
  return false;
}

Window::Window(const Model& fx, const score::DocumentContext& ctx, QWidget* parent)
    : PluginWindow{ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop(), parent}
    , m_model{fx}
{
  if(!fx.plugin)
    throw std::runtime_error("Cannot create UI");

  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  // Not created when LV2 support is disabled (SCORE_DISABLE_AUDIOPLUGINS /
  // SCORE_DISABLE_LV2); suil dereferences it without checking
  if(!p.lv2_context->ui_host)
    throw std::runtime_error("LV2 UI host not available");
  auto lay = new score::MarginLess<QHBoxLayout>;
  setLayout(lay);

  // Find a relevant ui: among the ones suil can host, keep the
  // best-supported (lowest wrapping quality, lilv semantics) whose binary
  // actually exists on disk. A bundle may declare UIs that are not shipped -
  // qmidiarp's ttl lists an OpenGL UI first whose .so is absent from the
  // distribution package; picking it blindly made the whole UI fail with a
  // missing-file error even though the X11 UI right after it works.
  const auto native_ui_type_uri = "http://lv2plug.in/ns/extensions/ui#Qt6UI";
  {
    auto the_uis = lilv_plugin_get_uis(fx.plugin);
    auto native_ui_type = lilv_new_uri(p.lilv.me, native_ui_type_uri);
    unsigned best_quality = 0; // lilv: 0 = unsupported, 1 = native, 2+ = wrapped
    LILV_FOREACH(uis, u, the_uis)
    {
      const LilvUI* this_ui = lilv_uis_get(the_uis, u);
      const LilvNode* ui_type{};
      const unsigned quality
          = lilv_ui_is_supported(this_ui, p.suil.ui_supported, native_ui_type, &ui_type);
      if(quality == 0 || (best_quality != 0 && quality >= best_quality))
        continue;

      if(const char* binary_uri
         = lilv_node_as_uri(lilv_ui_get_binary_uri(this_ui)))
      {
        char* binary_path = lilv_file_uri_parse(binary_uri, nullptr);
        const QString path = QString::fromUtf8(binary_path ? binary_path : "");
        lilv_free(binary_path);
        if(path.isEmpty() || !QFile::exists(path))
        {
          qDebug() << "LV2: skipping UI with missing binary:" << binary_uri;
          continue;
        }
        if(uiLinksIncompatibleQt(path))
        {
          qDebug() << "LV2: skipping UI linked against an incompatible Qt "
                      "version (cannot be embedded in this process):"
                   << binary_uri;
          continue;
        }
      }

      best_quality = quality;
      fx.effectContext.ui = this_ui;
      fx.effectContext.ui_type = ui_type;
    }
  }
  if(!fx.effectContext.ui)
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

  auto& suil = plug.suil;
  fx.effectContext.ui_instance = suil.instance_new(
      plug.lv2_context->ui_host, (Model*)&fx, native_ui_type_uri,
      lilv_node_as_uri(lilv_plugin_get_uri(fx.effectContext.plugin)),
      lilv_node_as_uri(lilv_ui_get_uri(fx.effectContext.ui)),
      lilv_node_as_uri(fx.effectContext.ui_type), bundle_path, binary_path, ui_features);

  lilv_free(binary_path);
  lilv_free(bundle_path);

  if(!fx.effectContext.ui_instance)
    throw std::runtime_error("UI creation error");

  // Setup the widget stuff
  auto widget = (QWidget*)suil.instance_get_widget(fx.effectContext.ui_instance);

  const int default_w = widget->width();
  const int default_h = widget->height();

  lay->addWidget(widget);
  m_widget = widget;
  {
    auto name = lilv_plugin_get_name(fx.plugin);
    setWindowTitle(lilv_node_as_string(name));
    lilv_node_free(name);
  }

  // Set up regular updates
  QPointer<const Model> fx_ptr{&fx};
  connect(&ctx.coarseUpdateTimer, &QTimer::timeout, this, [&, fx_ptr] {
    // score -> UI
    if(!fx_ptr)
      return;

    {
      Message ev;
      while(fx.plugin_events.try_dequeue(ev))
      {
        suil.instance_port_event(
            fx.effectContext.ui_instance, ev.index, ev.body.size(), ev.protocol,
            ev.body.data());
      }
    }

    // UI -> score
    {
      auto& plug = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
      Message ev;
      while(fx.ui_events.try_dequeue(ev))
      {
        if(ev.protocol == 0)
        {
          SCORE_ASSERT(ev.body.size() == sizeof(float));

          auto it = fx.control_map.find(ev.index);
          if(it != fx.control_map.end())
          {
            auto port = fx.control_map.at(ev.index).first;
            SCORE_ASSERT(port);

            float f = *(float*)ev.body.data();
            port->setValue(f);
          }
          else
          {
            fx.to_process_events.enqueue(std::move(ev));
          }
        }
        else if(ev.protocol == plug.lv2_host_context.atom_eventTransfer)
        {
          fx.to_process_events.enqueue(std::move(ev));
        }
        else
        {
          qDebug() << "LV2: Unknown protocol" << ev.protocol;
        }
      }
    }
  });

  // Set initial control port values
  // TODO not good, because not all available controls are created as score ports
  for(auto& e : fx.control_map)
  {
    float f = ossia::convert<float>(e.second.first->value());
    suil.instance_port_event(
        fx.effectContext.ui_instance, e.first, sizeof(float), 0, &f);
  }

  // Show ui and resize
  QTimer::singleShot(0, [this, widget, default_w, default_h, &p] {
    if(!is_resizable(p.lilv.me, *m_model.effectContext.ui))
    {
      widget->setMinimumSize(default_w, default_h);
      widget->setMaximumSize(default_w, default_h);
      adjustSize();
      setFixedSize(width(), height());
    }
    else
    {
      using namespace std;

      resize(min(1920, default_w), min(1080, default_h));
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
  if(m_widget)
    m_widget->setParent(nullptr);

  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();

  p.suil.instance_free(m_model.effectContext.ui_instance);
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
  Lilv::Nodes nrs_matches
      = plug.lilv.find_nodes(s, h.optional_feature, h.no_user_resize);

  return fs_matches.me == nullptr && nrs_matches.me == nullptr;
}
}
