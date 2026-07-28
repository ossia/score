#include "EffectModel.hpp"

#include "lv2_atom_helpers.hpp"

#include <Process/Dataflow/WidgetInlets.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/Node.hpp>
#include <LV2/Window.hpp>

#include <score/tools/Bind.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QListWidget>
#include <QUrl>

#include <cmath>
#include <wobjectimpl.h>

#include <memory>
#include <set>
W_OBJECT_IMPL(LV2::Model)
namespace LV2
{
// state:port-restore callback; canonical type dispatch in Ardour lv2_plugin.cc:313
static void lv2_set_port_value(
    const char* port_symbol, void* user_data, const void* value,
    uint32_t /*size*/, uint32_t type)
{
  auto* self = static_cast<LV2::Model*>(user_data);
  if(!self || !self->plugin || !self->hostContext || !port_symbol || !value)
    return;

  LilvWorld* world = self->hostContext->world.me;
  LilvNode* sym_node = lilv_new_uri(world, port_symbol);
  if(!sym_node)
    return;

  const LilvPort* port
      = lilv_plugin_get_port_by_symbol(self->plugin, sym_node);
  lilv_node_free(sym_node);
  if(!port)
    return;

  const uint32_t idx = lilv_port_get_index(self->plugin, port);
  auto it = self->control_map.find(idx);
  if(it == self->control_map.end() || !it->second.first)
    return;

  const auto& h = *self->hostContext;
  float fvalue = 0.f;
  if(type == h.atom_Float_id)
    fvalue = *static_cast<const float*>(value);
  else if(type == h.atom_Double_id)
    fvalue = static_cast<float>(*static_cast<const double*>(value));
  else if(type == h.atom_Int_id)
    fvalue = static_cast<float>(*static_cast<const int32_t*>(value));
  else if(type == h.atom_Long_id)
    fvalue = static_cast<float>(*static_cast<const int64_t*>(value));
  else
  {
    return;
  }

  // suil port-event guard: on_uiMessage drops UI writes when we're writing
  it->second.second = true;
  it->second.first->setValue(ossia::value{fvalue});
  it->second.second = false;
}
QString get_lv2_plugin_name(const Lilv::Plugin& node)
{
  QString ret;
  if(auto pname = lilv_plugin_get_name(node.me))
  {
    if(auto str = lilv_node_as_string(pname))
      ret = QString::fromUtf8(str);
    lilv_node_free(pname);
  }
  return ret;
};
namespace
{
// Cleared on descriptorsChanged: rescan invalidates the LilvPlugin* values
ossia::hash_map<QString, const LilvPlugin*>& plug_map_instance()
{
  static ossia::hash_map<QString, const LilvPlugin*> m;
  return m;
}
}

void clearPluginCache()
{
  plug_map_instance().clear();
}

std::optional<Lilv::Plugin> find_lv2_plugin(Lilv::World& world, QString path)
{
  auto& plug_map = plug_map_instance();
  if(auto it = plug_map.find(path); it != plug_map.end())
  {
    if(it->second)
      return it->second;
    else
      return {};
  }

  auto old_str = path;

  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  const LV2::PluginInfo* info = app_plug.findDescriptor(path);

  if(info)
    app_plug.ensureBundleLoaded(info->bundle);
  else
    app_plug.ensureBundleLoaded(path); // path might be a bundle URI / path

  const bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
  if(isFile)
  {
    if(*path.rbegin() != '/')
      path.append('/');
  }

  auto plugs = world.get_all_plugins();
  auto it = plugs.begin();
  while(!plugs.is_end(it))
  {
    auto plug = plugs.get(it);
    const auto plugin_name = get_lv2_plugin_name(plug);
    const QString plugin_uri = QString::fromUtf8(plug.get_uri().as_string());
    if(info && plugin_uri == info->uri)
    {
      plug_map[old_str] = plug;
      return plug;
    }
    if((isFile && QString(plug.get_bundle_uri().as_string()) == path)
       || (!isFile && plugin_name == path) || (!isFile && plugin_uri == old_str))
    {
      plug_map[old_str] = plug;
      return plug;
    }
    else
    {
      it = plugs.next(it);
    }
  }

  plug_map[old_str] = nullptr;
  return {};
}

struct LV2PluginChooserDialog : public QDialog
{
  LV2PluginChooserDialog(LV2::ApplicationPlugin& app, QWidget* parent)
      : QDialog{parent}
  {
    this->setLayout(&m_lay);
    this->window()->setWindowTitle(QObject::tr("Select a LV2 plug-in"));

    m_buttons.addButton(QDialogButtonBox::StandardButton::Ok);
    m_buttons.addButton(QDialogButtonBox::StandardButton::Close);
    m_buttons.setOrientation(Qt::Vertical);

    m_lay.addWidget(&m_categories);
    m_lay.addWidget(&m_plugins);
    m_lay.addWidget(&m_buttons);

    for(const auto& info : app.cachedDescriptors())
    {
      if(!info.valid)
        continue;
      QString category
          = info.class_label.isEmpty() ? QStringLiteral("Other") : info.class_label;
      m_categories_map[category].push_back({info.name, info.uri});
    }

    for(auto& category : m_categories_map)
      m_categories.addItem(category.first);

    con(m_categories, &QListWidget::currentTextChanged, this,
        &LV2PluginChooserDialog::updateProcesses);

    auto accept_item = [&](QListWidgetItem* item) {
      if(item)
      {
        m_accepted = item->data(Qt::UserRole).toString();
        if(m_accepted.isEmpty())
          m_accepted = item->text();
        QDialog::close();
      }
    };
    con(m_plugins, &QListWidget::itemDoubleClicked, this, accept_item);

    con(m_buttons, &QDialogButtonBox::accepted, this,
        [this, accept_item] { accept_item(m_plugins.currentItem()); });
  }

  void updateProcesses(const QString& str)
  {
    m_plugins.clear();
    for(const auto& [name, uri] : m_categories_map[str])
    {
      auto* item = new QListWidgetItem(name);
      item->setData(Qt::UserRole, uri);
      m_plugins.addItem(item);
    }
  }

  QString m_accepted;
  QHBoxLayout m_lay;
  QListWidget m_categories;
  QListWidget m_plugins;
  QDialogButtonBox m_buttons;

  ossia::flat_map<QString, QVector<std::pair<QString, QString>>> m_categories_map;
};
}
namespace Process
{

template <>
QString EffectProcessFactory_T<LV2::Model>::customConstructionData() const noexcept
{
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();

  LV2::LV2PluginChooserDialog dial{app_plug, nullptr};
  dial.exec();
  return dial.m_accepted;
}
static Process::Descriptor make_descriptor(Lilv::Plugin plug)
{
  Process::Descriptor desc;
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  auto& host = app_plug.lv2_host_context;
  desc.prettyName = LV2::get_lv2_plugin_name(plug);
  desc.categoryText = plug.get_class().get_label().as_string();
  desc.description = plug.get_author_homepage().as_string();
  desc.author = plug.get_author_name().as_string();

  const auto numports = plug.get_num_ports();
  std::vector<Process::PortType> ins;
  std::vector<Process::PortType> outs;

  for(std::size_t i = 0; i < numports; i++)
  {
    Lilv::Port port = plug.get_port_by_index(i);

    if(port.is_a(host.audio_class))
    {
      if(port.is_a(host.input_class))
      {
        ins.push_back(Process::PortType::Audio);
      }
      else if(port.is_a(host.output_class))
      {
        outs.push_back(Process::PortType::Audio);
      }
      else
      {
        // cv_ports.push_back(i);
      }
    }
    else if(port.is_a(host.atom_class))
    {
      // TODO use  atom:supports midi:MidiEvent
      if(port.is_a(host.input_class))
      {
        ins.push_back(Process::PortType::Midi);
      }
      else if(port.is_a(host.output_class))
      {
        outs.push_back(Process::PortType::Midi);
      }
      else
      {
        // midi_other_ports.push_back(i);
      }
    }
    else if(port.is_a(host.cv_class))
    {
      // cv_ports.push_back(i);
    }
    else if(port.is_a(host.control_class))
    {
      if(port.is_a(host.input_class))
      {
        ins.push_back(Process::PortType::Message);
      }
      else if(port.is_a(host.output_class))
      {
        outs.push_back(Process::PortType::Message);
      }
      else
      {
        // control_other_ports.push_back(i);
      }
    }
    else
    {
      // control_other_ports.push_back(i);
    }
  }

  desc.inlets = ins;
  desc.outlets = outs;
  desc.documentationLink = plug.get_author_homepage().as_string();
  if(desc.documentationLink.isEmpty())
    desc.documentationLink = QUrl(
        "https://ossia.io/score-docs/processes/"
        "audio-plugins.html#common-formats-vst-vst3-lv2-jsfx");
  return desc;
}

template <>
Process::Descriptor EffectProcessFactory_T<LV2::Model>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  auto& p = safe_cast<const LV2::Model&>(d);
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  Process::Descriptor desc;
  if(auto plug = p.plugin)
    desc = make_descriptor(Lilv::Plugin{plug});
  else if(auto plug = LV2::find_lv2_plugin(app_plug.lilv, d.effect()))
    desc = make_descriptor(*plug);

  return desc;
}
template <>
Process::Descriptor
EffectProcessFactory_T<LV2::Model>::descriptor(QString d) const noexcept
{
  Process::Descriptor desc;
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  if(auto plug = LV2::find_lv2_plugin(app_plug.lilv, d))
    desc = make_descriptor(*plug);

  return desc;
}
}

namespace LV2
{

Model::Model(
    TimeVal t, const QString& path, const Id<Process::ProcessModel>& id, QObject* parent)
    : ProcessModel{t, id, "LV2Effect", parent}
    , m_effectPath{path}
{
  reload();
}

Model::~Model()
{
  if(externalUI)
  {
    auto w = reinterpret_cast<LV2::Window*>(externalUI);
    delete w;
  }
  // Voices may still hold refs; SharedInstance ensures last drop runs ~InstanceHandle
  effectContext.instance = nullptr;
  effectContext.instance_holder.reset();
}
QString Model::prettyName() const noexcept
{
  return metadata().getLabel();
}

std::vector<Process::Preset> Model::builtinPresets() const noexcept
{
  std::vector<Process::Preset> result;
  if(!plugin)
    return result;

  auto& app_plug
      = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  auto& host_ctx = app_plug.lv2_host_context;
  LilvWorld* world = host_ctx.world.me;

  // Pre-load preset bundles so lilv_plugin_get_related sees them
  if(auto* info = app_plug.findDescriptor(m_effectPath))
  {
    for(const auto& bundle : info->preset_bundles)
    {
      if(!bundle.isEmpty())
        app_plug.ensureBundleLoaded(bundle);
    }
    // Fall back to old "strip last URI component" heuristic for pre-preset_bundles caches
    if(info->preset_bundles.isEmpty())
    {
      for(const auto& preset_uri : info->preset_uris)
      {
        QString u = preset_uri;
        int slash = u.lastIndexOf('/');
        if(slash > 0)
          app_plug.ensureBundleLoaded(u.left(slash + 1));
      }
    }
  }

  LilvNode* pset_node = lilv_new_uri(world, LV2_PRESETS__Preset);
  LilvNode* rdfs_label_node
      = lilv_new_uri(world, "http://www.w3.org/2000/01/rdf-schema#label");

  if(LilvNodes* presets = lilv_plugin_get_related(plugin, pset_node))
  {
    result.reserve(lilv_nodes_size(presets));
    LILV_FOREACH(nodes, it, presets)
    {
      const LilvNode* preset_node = lilv_nodes_get(presets, it);
      // rdfs:seeAlso file (with the rdfs:label) is loaded via lilv_world_load_resource
      lilv_world_load_resource(world, preset_node);

      QString label;
      if(LilvNodes* labels = lilv_world_find_nodes(
             world, preset_node, rdfs_label_node, nullptr))
      {
        if(lilv_nodes_size(labels) > 0)
        {
          const LilvNode* l = lilv_nodes_get_first(labels);
          if(l)
            label = QString::fromUtf8(lilv_node_as_string(l));
        }
        lilv_nodes_free(labels);
      }

      Process::Preset p;
      p.name = label.isEmpty() ? QString::fromUtf8(lilv_node_as_uri(preset_node))
                               : label;
      p.key.key = this->concreteKey();
      p.key.effect = m_effectPath;
      // Store the URI; loadPreset resolves via lilv_state_new_from_world
      p.data = QByteArray(lilv_node_as_uri(preset_node));
      result.push_back(std::move(p));
    }
    lilv_nodes_free(presets);
  }

  lilv_node_free(rdfs_label_node);
  lilv_node_free(pset_node);
  return result;
}

void Model::loadPreset(const Process::Preset& preset)
{
  if(!plugin || !effectContext.instance || preset.data.isEmpty())
    return;
  if(!hostContext || !hostContext->global)
    return;

  auto& app_plug
      = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  LilvWorld* world = hostContext->world.me;
  auto& map = hostContext->global->map;

  // Built-in path: data is a URI; saved projects may reference unenumerated presets
  LilvState* state = nullptr;
  if(LilvNode* uri = lilv_new_uri(world, preset.data.constData()))
  {
    lilv_world_load_resource(world, uri);
    state = lilv_state_new_from_world(world, &map, uri);
    lilv_node_free(uri);
  }
  if(!state)
    state = lilv_state_new_from_string(world, &map, preset.data.constData());

  if(state)
  {
    lilv_state_restore(
        state, effectContext.instance, lv2_set_port_value, this,
        LV2_STATE_IS_PORTABLE, hostContext->features);
    lilv_state_free(state);
  }
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();
  p.key.effect = m_effectPath;
  if(!plugin || !effectContext.instance || !hostContext || !hostContext->global)
    return p;

  auto& global = *hostContext->global;
  LilvState* state = lilv_state_new_from_instance(
      plugin, effectContext.instance, &global.map, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, LV2_STATE_IS_PORTABLE, nullptr);
  if(!state)
    return p;

  char* str = lilv_state_to_string(
      hostContext->world.me, &global.map, &global.unmap, state,
      "urn:io.ossia:lv2preset", nullptr);
  if(str)
  {
    p.data = QByteArray(str);
    free(str);
  }
  lilv_state_free(state);
  return p;
}

bool Model::hasExternalUI() const noexcept
{
  if(!plugin)
    return false;

  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  auto& host_ui_uri = p.lv2_host_context.host_ui_type;
  auto the_uis = lilv_plugin_get_uis(plugin);

  bool supported = false;
  LILV_FOREACH(uis, u, the_uis)
  {
    const LilvUI* this_ui = lilv_uis_get(the_uis, u);
    supported |= lilv_ui_is_supported(
        this_ui, p.suil.ui_supported, host_ui_uri.me, &effectContext.ui_type);

    if(supported)
      break;
  }

  lilv_uis_free(the_uis);
  return supported;
}

void Model::readPlugin()
{
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  auto& h = app_plug.lv2_host_context;
  ossia::float_vector fParamMin, fParamMax, fParamInit, fOtherControls;

  LV2Data data{h, effectContext};

  const std::size_t audio_in_size = data.audio_in_ports.size();
  const std::size_t audio_out_size = data.audio_out_ports.size();

  const std::size_t in_size = data.control_in_ports.size();
  const std::size_t out_size = data.control_out_ports.size();
  /*const std::size_t midi_in_size = data.midi_in_ports.size();
  const std::size_t midi_out_size = data.midi_out_ports.size();
  */
  const std::size_t cv_size = data.cv_ports.size();
  /*
  const std::size_t other_size = data.control_other_ports.size();
  */
  const std::size_t num_ports = data.effect.plugin.get_num_ports();

  fParamMin.resize(num_ports);
  fParamMax.resize(num_ports);
  fParamInit.resize(num_ports);
  data.effect.plugin.get_port_ranges_float(
      fParamMin.data(), fParamMax.data(), fParamInit.data());

  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);

  int in_id = 0;
  int out_id = 0;

  auto portName = [&](int port_id) {
    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    return QString::fromUtf8(n.as_string());
  };

  // AUDIO
  if(audio_in_size > 0)
  {
    m_inlets.push_back(new Process::AudioInlet{
        portName(data.audio_in_ports[0]), Id<Process::Port>{in_id++}, this});
  }

  if(audio_out_size > 0)
  {
    auto out = new Process::AudioOutlet{
        portName(data.audio_out_ports[0]), Id<Process::Port>{out_id++}, this};
    out->setPropagate(true);
    m_outlets.push_back(out);
  }

  // CV

  for(int port_id : data.cv_ports)
  {
    m_inlets.push_back(
        new Process::AudioInlet{portName(port_id), Id<Process::Port>{in_id++}, this});
  }

  // MIDI
  for(int port_id : data.midi_in_ports)
  {
    auto port
        = new Process::MidiInlet{portName(port_id), Id<Process::Port>{in_id++}, this};
    m_inlets.push_back(port);
  }

  for(int port_id : data.midi_out_ports)
  {
    auto port
        = new Process::MidiOutlet{portName(port_id), Id<Process::Port>{out_id++}, this};

    m_outlets.push_back(port);
  }

  for(int port_id : data.atom_in_ports)
  {
    auto port
        = new Process::ValueInlet{portName(port_id), Id<Process::Port>{in_id++}, this};

    m_inlets.push_back(port);
  }

  for(int port_id : data.atom_out_ports)
  {
    auto port
        = new Process::ValueOutlet{portName(port_id), Id<Process::Port>{out_id++}, this};

    m_outlets.push_back(port);
  }

  m_controlInStart = in_id;
  // CONTROL
  for(int port_id : data.control_in_ports)
  {
    SCORE_ASSERT(port_id >= 0);
    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    QString port_name = QString::fromUtf8(n.as_string());
    Id<Process::Port> id{in_id++};
    const float pmin = fParamMin[port_id];
    const float pmax = fParamMax[port_id];
    const float pinit
        = std::isnan(fParamInit[port_id]) ? pmin : fParamInit[port_id];

    // Widget by portProperty: toggled, enumeration+scalePoint, integer, logarithmic, default
    Process::ControlInlet* port = nullptr;
    LilvNodes* props = lilv_port_get_properties(data.effect.plugin.me, p.me);
    const bool is_toggled = props && lilv_nodes_contains(props, h.toggled_property.me);
    const bool is_enum
        = props && lilv_nodes_contains(props, h.enumeration_property.me);
    const bool is_integer
        = props && lilv_nodes_contains(props, h.integer_property.me);
    const bool is_log
        = props && lilv_nodes_contains(props, h.logarithmic_property.me);
    if(props)
      lilv_nodes_free(props);

    if(is_toggled)
    {
      port = new Process::Toggle{pinit >= 0.5f, port_name, id, this};
    }
    else if(is_enum)
    {
      std::vector<std::pair<QString, ossia::value>> values;
      if(LilvScalePoints* sps
         = lilv_port_get_scale_points(data.effect.plugin.me, p.me))
      {
        LILV_FOREACH(scale_points, it, sps)
        {
          const LilvScalePoint* sp = lilv_scale_points_get(sps, it);
          const LilvNode* lbl = lilv_scale_point_get_label(sp);
          const LilvNode* val = lilv_scale_point_get_value(sp);
          if(lbl && val)
            values.emplace_back(
                QString::fromUtf8(lilv_node_as_string(lbl)),
                ossia::value{lilv_node_as_float(val)});
        }
        lilv_scale_points_free(sps);
      }
      if(!values.empty())
        port = new Process::ComboBox{
            std::move(values), ossia::value{pinit}, port_name, id, this};
    }
    if(!port && is_integer)
    {
      port = new Process::IntSlider{
          int(pmin), int(pmax), int(pinit), port_name, id, this};
    }
    if(!port && is_log && pmin > 0.f)
    {
      port = new Process::LogFloatSlider{pmin, pmax, pinit, port_name, id, this};
    }
    if(!port)
    {
      port = new Process::FloatSlider{pmin, pmax, pinit, port_name, id, this};
    }

    control_map.insert({(uint32_t)port_id, {port, false}});
    connect(
        port, &Process::ControlInlet::valueChanged, this,
        [this, port_id](const ossia::value& v) {
      if(effectContext.ui_instance)
      {
        auto& writing = control_map[port_id].second;
        writing = true;
        float f = ossia::convert<float>(v);

        static auto& suil = libsuil::instance();
        suil.instance_port_event(
            effectContext.ui_instance, port_id, sizeof(float), 0, &f);
        writing = false;
      }
        });

    m_inlets.push_back(port);
  }

  m_controlOutStart = in_id;
  for(int port_id : data.control_out_ports)
  {
    auto port = new Process::ControlOutlet{
        portName(port_id), Id<Process::Port>{out_id++}, this};
    port->displayHandledExplicitly = true;
    port->setDomain(
        State::Domain{ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    if(std::isnan(fParamInit[port_id]))
    {
      port->setValue(fParamMin[port_id]);
    }
    else
    {
      port->setValue(fParamInit[port_id]);
    }

    control_out_map.insert({port_id, port});

    m_outlets.push_back(port);
  }

  auto sr = app_plug.context.settings<Audio::Settings::Model>().getRate();
  if(auto* inst = lilv_plugin_instantiate(
         effectContext.plugin.me, sr, app_plug.lv2_host_context.features))
  {
    effectContext.instance_holder = std::make_shared<InstanceHandle>(inst);
    effectContext.instance = inst;
  }
  else
  {
    qDebug() << "Could not load LV2 plug-in";
    return;
  }

  // state:loadDefaultState (LV2 spec): apply ttl state:state triple between instantiate and run
  if(auto* required = lilv_plugin_get_required_features(effectContext.plugin.me))
  {
    if(lilv_nodes_contains(required, h.state_loadDefaultState_node.me))
    {
      LilvState* default_state = lilv_state_new_from_world(
          h.world.me, &app_plug.lv2_host_context.global->map,
          lilv_plugin_get_uri(effectContext.plugin.me));
      if(default_state)
      {
        lilv_state_restore(
            default_state, effectContext.instance, lv2_set_port_value, this,
            LV2_STATE_IS_PORTABLE, app_plug.lv2_host_context.features);
        lilv_state_free(default_state);
      }
    }
    lilv_nodes_free(required);
  }

  effectContext.data.data_access
      = lilv_instance_get_descriptor(effectContext.instance)->extension_data;

  fInControls.resize(in_size);
  fOutControls.resize(out_size);

  auto fInstance = effectContext.instance;
  for(std::size_t i = 0; i < in_size; i++)
  {
    lilv_instance_connect_port(fInstance, data.control_in_ports[i], &fInControls[i]);
  }

  for(std::size_t i = 0; i < out_size; i++)
  {
    lilv_instance_connect_port(fInstance, data.control_out_ports[i], &fOutControls[i]);
  }
}

void Model::reload()
{
  // MAIN THREAD ONLY: mutates the shared LilvWorld via ensureBundleLoaded
  effectContext.instance = nullptr;
  effectContext.instance_holder.reset();

  plugin = nullptr;

  auto path = m_effectPath;
  if(path.isEmpty())
    return;

  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  hostContext = &app_plug.lv2_host_context;
  auto& world = app_plug.lilv;

  if(auto plug = find_lv2_plugin(world, path))
  {
    plugin = plug->me;
    effectContext.plugin.me = *plug;
    readPlugin();
    QString name = plug->get_name().as_string();
    if(name.isEmpty())
      name = path.split("/").back();
    metadata().setName(name);
    metadata().setLabel(name);
  }
}
}

#include <Process/Dataflow/PortFactory.hpp>

static LilvState* state_from_instance(const LV2::Model& ctx)
{
  return lilv_state_new_from_instance(
      ctx.plugin, ctx.effectContext.instance, &ctx.hostContext->global->map, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, LV2_STATE_IS_PORTABLE, nullptr);
}

static QByteArray readLV2State(const LV2::Model& eff)
{
  if(!eff.plugin || !eff.effectContext.instance || !eff.hostContext
     || !eff.hostContext->global)
    return {};

  auto& global = *eff.hostContext->global;
  const auto state = state_from_instance(eff);
  if(!state)
    return {};

  auto str = lilv_state_to_string(
      eff.hostContext->world.me, &global.map, &global.unmap, state,
      "urn:io.ossia:lv2state", nullptr);

  QByteArray b;
  if(str)
  {
    b = QByteArray(str);
    free(str);
  }
  lilv_state_free(state);
  return b;
}

static void restoreLV2State(LV2::Model& eff, const QByteArray& str)
{
  if(!eff.plugin || str.isEmpty())
    return;

  auto& global = *eff.hostContext->global;
  const auto state = lilv_state_new_from_string(
      eff.hostContext->world.me, &global.map, str.constData());

  // lilv_state_restore is not NULL-safe
  if(!state)
    return;

  lilv_state_restore(
      state, eff.effectContext.instance, LV2::lv2_set_port_value, &eff,
      LV2_STATE_IS_PORTABLE, nullptr);

  lilv_state_free(state);
}

template <typename T>
static void restore_ports(const T& src, const T& dst)
{
  for(std::size_t i = 0; i < std::min(src.size(), dst.size()); ++i)
  {
    if(src[i]->type() == dst[i]->type())
    {
      // FIXME not efficient at all...
      dst[i]->loadData(src[i]->saveData());
    }
  }
}

template <>
void DataStreamReader::read(const LV2::Model& eff)
{
  m_stream << eff.effect();

  readPorts(*this, eff.m_inlets, eff.m_outlets);

  m_stream << readLV2State(eff);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(LV2::Model& eff)
{
  m_stream >> eff.m_effectPath;

  eff.reload();

  Process::Inlets inls;
  Process::Outlets outls;

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), inls, outls, &eff);

  {
    QByteArray str;
    m_stream >> str;
    restoreLV2State(eff, str);
  }

  {
    restore_ports(inls, eff.inlets());
    restore_ports(outls, eff.outlets());
    (void)score::clearAndDeleteLater(inls);
    (void)score::clearAndDeleteLater(outls);
  }

  checkDelimiter();
}

template <>
void JSONReader::read(const LV2::Model& eff)
{
  obj["Effect"] = eff.effect();
  obj["State"] = readLV2State(eff);
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(LV2::Model& eff)
{
  eff.m_effectPath = obj["Effect"].toString();

  eff.reload();

  Process::Inlets inls;
  Process::Outlets outls;
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), inls, outls, &eff);

  if(auto st = obj.tryGet("State"))
    restoreLV2State(eff, st->toByteArray());

  {
    restore_ports(inls, eff.inlets());
    restore_ports(outls, eff.outlets());
    (void)score::clearAndDeleteLater(inls);
    (void)score::clearAndDeleteLater(outls);
  }
}

namespace LV2
{
struct on_start;
struct on_finish;
using lv2_node_t = lv2_node<on_start, on_finish>;

struct on_start
{
  std::weak_ptr<LV2EffectComponent> self;
  void operator()();
};

struct on_finish
{
  std::weak_ptr<LV2EffectComponent> self;
  std::weak_ptr<Execution::EditionCommandQueue> q;
  void operator()();
};
void on_start::operator()()
{
  auto p = self.lock();
  if(!p)
    return;
  auto nn = p->node;
  if(!nn)
    return;
  auto& node = *static_cast<lv2_node_t*>(nn.get());
  auto& proc = p->process().to_process_events;

  LV2::Message msg;
  while(proc.try_dequeue(msg))
  {
    // Broadcast to every voice: per_channel needs the same MIDI on all instances
    for(std::size_t k = 0; k < node.data.midi_in_ports.size(); ++k)
    {
      if(node.data.midi_in_ports[k] == int32_t(msg.index))
      {
        for(auto& v : node.voices)
          v->message_for_midi_atom_ins[k].push_back(msg);
        break;
      }
    }
  }
}
void on_finish::operator()()
{
  auto p = self.lock();
  if(!p)
    return;
  auto q = this->q.lock();
  if(!q)
    return;

  q->enqueue([s = self] {
    auto p = s.lock();

    if(!p)
      return;
    auto nn = p->node;
    if(!nn)
      return;
    auto& node = *static_cast<lv2_node_t*>(nn.get());
    if(node.voices.empty())
      return;
    auto& v0 = *node.voices[0];

    for(std::size_t k = 0; k < node.data.control_out_ports.size(); k++)
    {
      auto port = (uint32_t)node.data.control_out_ports[k];
      float val = v0.fOutControls[k];

      auto cport = static_cast<Model&>(p->process()).control_out_map[port];
      SCORE_ASSERT(cport);
      cport->setValue(val);
    }
  });

  auto nn = p->node;
  if(!nn)
    return;
  auto& node = *static_cast<lv2_node_t*>(nn.get());
  if(node.voices.empty())
    return;
  auto& v0 = *node.voices[0];

  for(std::size_t k = 0; k < node.data.midi_out_ports.size(); ++k)
  {
    int port_index = node.data.midi_out_ports[k];
    auto& buf = v0.midi_atom_outs[k];

    LV2_ATOM_SEQUENCE_FOREACH(&buf.buf->atoms, ev)
    {
      p->writeAtomToUi(port_index, ev->body);
    }
  }
}

LV2EffectComponent::LV2EffectComponent(
    LV2::Model& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "LV2Component", parent}
{
}

void LV2EffectComponent::lazy_init()
{
  auto& ctx = system();
  LV2::Model& proc = process();

  if(!proc.effectContext.instance)
    return;

  auto& host = ctx.context().doc.app.applicationPlugin<LV2::ApplicationPlugin>();
  on_start os;
  on_finish of;
  os.self = std::dynamic_pointer_cast<LV2EffectComponent>(shared_from_this());
  of.self = os.self;
  of.q = ctx.weakEditionQueue();

  LV2::LV2Data lv2data{host.lv2_host_context, proc.effectContext};
  auto strategy = LV2::choose_voice_strategy(lv2data);

  auto node = ossia::make_node<LV2::lv2_node_t>(
      *ctx.execState, lv2data, ctx.execState->sampleRate, strategy, os, of);

  for(std::size_t i = proc.m_controlInStart; i < proc.inlets().size(); i++)
  {
    auto* inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    const auto ctrl_idx = i - proc.m_controlInStart;
    const float init_val = ossia::convert<float>(inlet->value());

    for(auto& v : node->voices)
      if(ctrl_idx < v->fInControls.size())
        v->fInControls[ctrl_idx] = init_val;

    auto inl = node->root_inputs()[i];
    auto& vp = *inl->target<ossia::value_port>();
    vp.type = inlet->value().get_type();
    vp.domain = inlet->domain().get();

    connect(
        inlet, &Process::ControlInlet::valueChanged, this,
        [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
        });
  }

  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  // Grow voice pool on main thread (lilv_plugin_instantiate can dlopen / preload samples)
  if(strategy.routing == LV2::voice_routing::per_channel)
  {
    m_pool_sample_rate = ctx.execState->sampleRate;
    m_pool_pushed = node->voices.size();
    m_pool_max_requested = m_pool_pushed;

    m_grow_timer = new QTimer(this);
    m_grow_timer->setInterval(50);
    connect(m_grow_timer, &QTimer::timeout, this, [this] { growVoicePoolTick(); });
    m_grow_timer->start();
  }
}

void LV2EffectComponent::growVoicePoolTick()
{
  auto node_base = this->node;
  if(!node_base)
    return;
  auto* node = static_cast<lv2_node_t*>(node_base.get());

  // High-water mark catches transients the audio thread smoothed (2 -> 64 -> 2)
  const auto requested
      = node->requested_voices.load(std::memory_order_acquire);
  if(requested > m_pool_max_requested)
    m_pool_max_requested = requested;

  if(m_pool_pushed >= m_pool_max_requested)
    return;

  // Snapshot voice 0 so new instances inherit loaded samples / ongoing edits
  LilvState* state = node->snapshot_state_raw();

  // 50 ms x 4 = ~80 voices/s; samplers may preload banks per instantiation
  constexpr std::size_t kBatch = 4;
  const std::size_t to_add
      = std::min(kBatch, m_pool_max_requested - m_pool_pushed);

  auto weak_node = std::weak_ptr{this->node};
  for(std::size_t i = 0; i < to_add; ++i)
  {
    auto v = node->make_voice_for_pool(m_pool_sample_rate, state);
    if(!v)
    {
      qWarning() << "LV2: failed to grow voice pool";
      break;
    }
    // Voice cleanup is main-thread-only; bounce back if the node died
    in_exec([weak_node, v = std::move(v)]() mutable {
      if(auto n = weak_node.lock())
      {
        static_cast<lv2_node_t*>(n.get())->append_voice(std::move(v));
      }
      else
      {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [v = std::move(v)]() mutable { v.reset(); });
      }
    });
    ++m_pool_pushed;
  }

  if(state)
    lilv_state_free(state);
}

void LV2EffectComponent::writeAtomToUi(
    uint32_t port_index, uint32_t type, uint32_t size, const void* body)
{
  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  LV2::Message ev;
  ev.index = port_index;
  ev.protocol = p.lv2_host_context.atom_eventTransfer;
  ev.body.resize(sizeof(LV2_Atom) + size);

  {
    LV2_Atom* atom = reinterpret_cast<LV2_Atom*>(ev.body.data());
    atom->type = type;
    atom->size = size;
  }

  {
    uint8_t* data = reinterpret_cast<uint8_t*>(ev.body.data() + sizeof(LV2_Atom));

    auto b = (const uint8_t*)body;
    for(uint32_t i = 0; i < size; i++)
      data[i] = b[i];
  }

  process().plugin_events.enqueue(std::move(ev));
}

void LV2EffectComponent::writeAtomToUi(uint32_t port_index, LV2_Atom& atom)
{
  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();

  LV2::Message ev;
  ev.index = port_index;
  ev.protocol = p.lv2_host_context.atom_eventTransfer;
  int message_bytes = sizeof(LV2_Atom) + atom.size;
  ev.body.resize(message_bytes);
  memcpy(ev.body.data(), &atom, message_bytes);

  process().plugin_events.enqueue(std::move(ev));
}
}
W_OBJECT_IMPL(LV2::LV2EffectComponent)
