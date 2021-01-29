#include "EffectModel.hpp"

#include "lv2_atom_helpers.hpp"

#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/Node.hpp>
#include <LV2/Window.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/tools/Bind.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QListWidget>
#include <QUrl>

#include <Execution/DocumentPlugin.hpp>
#include <cmath>
#include <wobjectimpl.h>

#include <memory>
#include <set>
W_OBJECT_IMPL(LV2::Model)
namespace LV2
{
std::optional<Lilv::Plugin> find_lv2_plugin(Lilv::World& world, QString path)
{
  static tsl::hopscotch_map<QString, const LilvPlugin*> plug_map;
  if (auto it = plug_map.find(path); it != plug_map.end())
  {
    if (it->second)
      return it->second;
    else
      return {};
  }

  auto old_str = path;
  const bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
  if (isFile)
  {
    if (*path.rbegin() != '/')
      path.append('/');
  }

  auto plugs = world.get_all_plugins();
  auto it = plugs.begin();
  while (!plugs.is_end(it))
  {
    auto plug = plugs.get(it);
    if ((isFile && QString(plug.get_bundle_uri().as_string()) == path)
        || (!isFile && QString(plug.get_name().as_string()) == path))
    {
      plug_map[old_str] = plug;
      return plug;
    }
    else if (!isFile && QString(plug.get_name().as_string()) == path)
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
  LV2PluginChooserDialog(Lilv::World& world, QWidget* parent) : QDialog{parent}
  {
    this->setLayout(&m_lay);
    this->window()->setWindowTitle(QObject::tr("Select a LV2 plug-in"));

    m_buttons.addButton(QDialogButtonBox::StandardButton::Ok);
    m_buttons.addButton(QDialogButtonBox::StandardButton::Close);
    m_buttons.setOrientation(Qt::Vertical);

    m_lay.addWidget(&m_categories);
    m_lay.addWidget(&m_plugins);
    m_lay.addWidget(&m_buttons);
    auto plugs = world.get_all_plugins();

    QStringList items;

    auto it = plugs.begin();
    while (!plugs.is_end(it))
    {
      auto plug = plugs.get(it);
      const auto class_name = plug.get_class().get_label().as_string();
      const auto plug_name = plug.get_name().as_string();
      auto sub_it = m_categories_map.find(class_name);
      if (sub_it == m_categories_map.end())
      {
        m_categories_map.insert({class_name, {plug_name}});
      }
      else
      {
        sub_it->second.append(plug_name);
      }
      it = plugs.next(it);
    }

    for (auto& category : m_categories_map)
    {
      m_categories.addItem(category.first);
    }

    con(m_categories,
        &QListWidget::currentTextChanged,
        this,
        &LV2PluginChooserDialog::updateProcesses);

    auto accept_item = [&](auto item) {
      if (item)
      {
        m_accepted = item->text();
        QDialog::close();
      }
    };
    con(m_plugins, &QListWidget::itemDoubleClicked, this, accept_item);

    con(m_buttons, &QDialogButtonBox::accepted, this, [=] {
      accept_item(m_plugins.currentItem());
    });
  }

  void updateProcesses(const QString& str)
  {
    m_plugins.clear();
    for (const auto& plug : m_categories_map[str])
    {
      m_plugins.addItem(plug);
    }
  }

  QString m_accepted;
  QHBoxLayout m_lay;
  QListWidget m_categories;
  QListWidget m_plugins;
  QDialogButtonBox m_buttons;

  std::map<QString, QVector<QString>> m_categories_map;
};
}
namespace Process
{

template <>
QString EffectProcessFactory_T<LV2::Model>::customConstructionData() const
{
  auto& world = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>().lilv;

  LV2::LV2PluginChooserDialog dial{world, nullptr};
  dial.exec();
  return dial.m_accepted;
}

template <>
Process::Descriptor EffectProcessFactory_T<LV2::Model>::descriptor(QString d) const
{
  Process::Descriptor desc;
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  auto& host = app_plug.lv2_host_context;
  if (auto plug = LV2::find_lv2_plugin(app_plug.lilv, d))
  {
    desc.prettyName = plug->get_name().as_string();
    desc.categoryText = plug->get_class().get_label().as_string();
    desc.description = plug->get_author_homepage().as_string();
    desc.author = plug->get_author_name().as_string();

    const auto numports = plug->get_num_ports();
    std::vector<Process::PortType> ins;
    std::vector<Process::PortType> outs;

    for (std::size_t i = 0; i < numports; i++)
    {
      Lilv::Port port = plug->get_port_by_index(i);

      if (port.is_a(host.audio_class))
      {
        if (port.is_a(host.input_class))
        {
          ins.push_back(Process::PortType::Audio);
        }
        else if (port.is_a(host.output_class))
        {
          outs.push_back(Process::PortType::Audio);
        }
        else
        {
          // cv_ports.push_back(i);
        }
      }
      else if (port.is_a(host.atom_class))
      {
        // TODO use  atom:supports midi:MidiEvent
        if (port.is_a(host.input_class))
        {
          ins.push_back(Process::PortType::Midi);
        }
        else if (port.is_a(host.output_class))
        {
          outs.push_back(Process::PortType::Midi);
        }
        else
        {
          // midi_other_ports.push_back(i);
        }
      }
      else if (port.is_a(host.cv_class))
      {
        // cv_ports.push_back(i);
      }
      else if (port.is_a(host.control_class))
      {
        if (port.is_a(host.input_class))
        {
          ins.push_back(Process::PortType::Message);
        }
        else if (port.is_a(host.output_class))
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
  }
  return desc;
}
}

namespace LV2
{

Model::Model(
    TimeVal t,
    const QString& path,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : ProcessModel{t, id, "LV2Effect", parent}, m_effectPath{path}
{
  reload();
}

Model::~Model()
{
  if (externalUI)
  {
    auto w = reinterpret_cast<LV2::Window*>(externalUI);
    delete w;
  }
}
QString Model::prettyName() const noexcept
{
  return metadata().getLabel();
}

bool Model::hasExternalUI() const noexcept
{
  if (!plugin)
    return false;

  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  const auto native_ui_type_uri = "http://lv2plug.in/ns/extensions/ui#Qt5UI";
  auto the_uis = lilv_plugin_get_uis(plugin);
  auto native_ui_type = lilv_new_uri(p.lilv.me, native_ui_type_uri);
  LILV_FOREACH(uis, u, the_uis)
  {
    const LilvUI* this_ui = lilv_uis_get(the_uis, u);
    if (lilv_ui_is_supported(this_ui, suil_ui_supported, native_ui_type, &effectContext.ui_type))
    {
      return true;
    }
  }

  return false;
}

void Model::readPlugin()
{
  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  auto& h = app_plug.lv2_host_context;
  ossia::float_vector fInControls, fOutControls, fParamMin, fParamMax, fParamInit, fOtherControls;

  LV2Data data{h, effectContext};

  const std::size_t audio_in_size = data.audio_in_ports.size();
  const std::size_t audio_out_size = data.audio_out_ports.size();
  /*
  const std::size_t in_size = data.control_in_ports.size();
  const std::size_t out_size = data.control_out_ports.size();
  const std::size_t midi_in_size = data.midi_in_ports.size();
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
  data.effect.plugin.get_port_ranges_float(fParamMin.data(), fParamMax.data(), fParamInit.data());

  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);

  int in_id = 0;
  int out_id = 0;

  // AUDIO
  if (audio_in_size > 0)
  {
    m_inlets.push_back(new Process::AudioInlet{Id<Process::Port>{in_id++}, this});
  }

  if (audio_out_size > 0)
  {
    auto out = new Process::AudioOutlet{Id<Process::Port>{out_id++}, this};
    out->setPropagate(true);
    m_outlets.push_back(out);
  }

  // CV
  for (std::size_t i = 0; i < cv_size; i++)
  {
    m_inlets.push_back(new Process::AudioInlet{Id<Process::Port>{in_id++}, this});
  }

  // MIDI
  for (int port_id : data.midi_in_ports)
  {
    auto port = new Process::MidiInlet{Id<Process::Port>{in_id++}, this};

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_inlets.push_back(port);
  }

  for (int port_id : data.midi_out_ports)
  {
    auto port = new Process::MidiOutlet{Id<Process::Port>{out_id++}, this};

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_outlets.push_back(port);
  }

  m_controlInStart = in_id;
  // CONTROL
  for (int port_id : data.control_in_ports)
  {
    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();

    auto port = new Process::FloatSlider{
        fParamMin[port_id],
        fParamMax[port_id],
        fParamInit[port_id],
        QString::fromUtf8(n.as_string()),
        Id<Process::Port>{in_id++},
        this};

    control_map.insert({port_id, {port, false}});
    connect(
        port, &Process::ControlInlet::valueChanged, this, [this, port_id](const ossia::value& v) {
          if (effectContext.ui_instance)
          {
            auto& writing = control_map[port_id].second;
            writing = true;
            float f = ossia::convert<float>(v);
            suil_instance_port_event(effectContext.ui_instance, port_id, sizeof(float), 0, &f);
            writing = false;
          }
        });

    m_inlets.push_back(port);
  }

  m_controlOutStart = in_id;
  for (int port_id : data.control_out_ports)
  {
    auto port = new Process::ControlOutlet{Id<Process::Port>{out_id++}, this};
    port->hidden = true;
    port->setDomain(State::Domain{ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    port->setValue(fParamInit[port_id]);

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));
    control_out_map.insert({port_id, port});

    m_outlets.push_back(port);
  }

  effectContext.instance = lilv_plugin_instantiate(
      effectContext.plugin.me,
      app_plug.lv2_context->sampleRate,
      app_plug.lv2_host_context.features);

  if (!effectContext.instance)
  {
    qDebug() << "Could not load LV2 plug-in";
    return;
  }

  effectContext.data.data_access
      = lilv_instance_get_descriptor(effectContext.instance)->extension_data;
}

void Model::reload()
{
  plugin = nullptr;

  auto path = m_effectPath;
  if (path.isEmpty())
    return;

  auto& app_plug = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  auto& world = app_plug.lilv;

  if (auto plug = find_lv2_plugin(world, path))
  {
    plugin = plug->me;
    effectContext.plugin.me = *plug;
    readPlugin();
    metadata().setLabel(QString(plug->get_name().as_string()));
  }
}
}

template <>
void DataStreamReader::read(const LV2::Model& eff)
{
  m_stream << eff.effect();
  insertDelimiter();
}

template <>
void DataStreamWriter::write(LV2::Model& eff)
{
  m_stream >> eff.m_effectPath;
  checkDelimiter();
}

template <>
void JSONReader::read(const LV2::Model& eff)
{
  obj["Effect"] = eff.effect();
}

template <>
void JSONWriter::write(LV2::Model& eff)
{
  eff.m_effectPath = obj["Effect"].toString();
}

namespace LV2
{
struct on_finish
{
  std::weak_ptr<Execution::ProcessComponent> self;
  void operator()()
  {
    auto p = self.lock();
    if (!p)
      return;

    p->in_edit([s = self] {
      auto p = s.lock();

      if (!p)
        return;
      auto nn = p->node;
      if (!nn)
        return;
      auto& node = *static_cast<lv2_node<on_finish>*>(nn.get());

      for (std::size_t k = 0; k < node.data.control_out_ports.size(); k++)
      {
        auto port = (uint32_t)node.data.control_out_ports[k];
        float val = node.fOutControls[k];

        auto cport = static_cast<Model&>(p->process()).control_out_map[port];
        SCORE_ASSERT(cport);
        cport->setValue(val);
      }
    });

    /* TODO do the same thing than in jalv
    for(auto& port : data.event_out_port)
    {
      for (LV2_Evbuf_Iterator i = lv2_evbuf_begin(port->evbuf);
           lv2_evbuf_is_valid(i);
           i = lv2_evbuf_next(i)) {
        // Get event from LV2 buffer
        uint32_t frames, subframes, type, size;
        uint8_t* body;
        lv2_evbuf_get(i, &frames, &subframes, &type, &size, &body);

        if (jalv->has_ui && !port->old_api) {
          // Forward event to UI
          writeAtomToUi(jalv, p, type, size, body);
        }
      }
    }
    */
  }
};

LV2EffectComponent::LV2EffectComponent(
    LV2::Model& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "LV2Component", parent}
{
}

void LV2EffectComponent::lazy_init()
{
  auto& ctx = system();
  LV2::Model& proc = process();

  if (!proc.effectContext.instance)
    return;

  auto& host = ctx.context().doc.app.applicationPlugin<LV2::ApplicationPlugin>();
  on_finish of;
  of.self = shared_from_this();

  auto node = std::make_shared<LV2::lv2_node<on_finish>>(
      LV2::LV2Data{host.lv2_host_context, proc.effectContext},
      ctx.execState->sampleRate,
      of);

  for (std::size_t i = proc.m_controlInStart; i < proc.inlets().size(); i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    node->fInControls[i - proc.m_controlInStart] = ossia::convert<float>(inlet->value());
    auto inl = node->root_inputs()[i];
    connect(inlet, &Process::ControlInlet::valueChanged, this, [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
    });
  }

  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);
}

void LV2EffectComponent::writeAtomToUi(
    uint32_t port_index,
    uint32_t type,
    uint32_t size,
    const void* body)
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
    for (uint32_t i = 0; i < size; i++)
      data[i] = b[i];
  }

  process().plugin_events.enqueue(std::move(ev));
}
}
W_OBJECT_IMPL(LV2::LV2EffectComponent)
