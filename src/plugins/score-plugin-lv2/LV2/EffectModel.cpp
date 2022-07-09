#include "EffectModel.hpp"

#include "lv2_atom_helpers.hpp"

#include <Execution/DocumentPlugin.hpp>
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/Node.hpp>
#include <LV2/Window.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/tools/Bind.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/std/StringHash.hpp>

#include <Audio/Settings/Model.hpp>
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
  const bool isFile
      = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
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
  LV2PluginChooserDialog(Lilv::World& world, QWidget* parent)
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
QString EffectProcessFactory_T<LV2::Model>::customConstructionData() const noexcept
{
  auto& world = score::AppComponents()
                    .applicationPlugin<LV2::ApplicationPlugin>()
                    .lilv;

  LV2::LV2PluginChooserDialog dial{world, nullptr};
  dial.exec();
  return dial.m_accepted;
}

template <>
Process::Descriptor
EffectProcessFactory_T<LV2::Model>::descriptor(QString d) const noexcept
{
  Process::Descriptor desc;
  auto& app_plug
      = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
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
    : ProcessModel{t, id, "LV2Effect", parent}
    , m_effectPath{path}
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
    if (lilv_ui_is_supported(
            this_ui,
            p.suil.ui_supported,
            native_ui_type,
            &effectContext.ui_type))
    {
      return true;
    }
  }

  return false;
}

void Model::readPlugin()
{
  auto& app_plug
      = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
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

  // AUDIO
  if (audio_in_size > 0)
  {
    m_inlets.push_back(
        new Process::AudioInlet{Id<Process::Port>{in_id++}, this});
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
    m_inlets.push_back(
        new Process::AudioInlet{Id<Process::Port>{in_id++}, this});
  }

  // MIDI
  for (int port_id : data.midi_in_ports)
  {
    auto port = new Process::MidiInlet{Id<Process::Port>{in_id++}, this};

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setName(QString::fromUtf8(n.as_string()));

    m_inlets.push_back(port);
  }

  for (int port_id : data.midi_out_ports)
  {
    auto port = new Process::MidiOutlet{Id<Process::Port>{out_id++}, this};

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setName(QString::fromUtf8(n.as_string()));

    m_outlets.push_back(port);
  }

  m_controlInStart = in_id;
  // CONTROL
  // FIXME if(data.control_in_ports.size() < 10)
  {
    for (int port_id : data.control_in_ports)
    {
      Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
      Lilv::Node n = p.get_name();

      SCORE_SOFT_ASSERT(!std::isnan(fParamInit[port_id]));
      auto port = new Process::FloatSlider{
          fParamMin[port_id],
          fParamMax[port_id],
          fParamInit[port_id],
          QString::fromUtf8(n.as_string()),
          Id<Process::Port>{in_id++},
          this};

      control_map.insert({port_id, {port, false}});
      connect(
          port,
          &Process::ControlInlet::valueChanged,
          this,
          [this, port_id](const ossia::value& v) {
            if (effectContext.ui_instance)
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
  }

  m_controlOutStart = in_id;
  for (int port_id : data.control_out_ports)
  {
    auto port = new Process::ControlOutlet{Id<Process::Port>{out_id++}, this};
    port->hidden = true;
    port->setDomain(State::Domain{
        ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    if (std::isnan(fParamInit[port_id]))
    {
      port->setValue(fParamMin[port_id]);
    }
    else
    {
      port->setValue(fParamInit[port_id]);
    }

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setName(QString::fromUtf8(n.as_string()));
    control_out_map.insert({port_id, port});

    m_outlets.push_back(port);
  }

  auto sr = app_plug.context.settings<Audio::Settings::Model>().getRate();
  effectContext.instance = lilv_plugin_instantiate(
      effectContext.plugin.me,
      sr,
      app_plug.lv2_host_context.features);

  if (!effectContext.instance)
  {
    qDebug() << "Could not load LV2 plug-in";
    return;
  }

  effectContext.data.data_access
      = lilv_instance_get_descriptor(effectContext.instance)->extension_data;

  fInControls.resize(in_size);
  fOutControls.resize(out_size);

  auto fInstance = effectContext.instance;
  for (std::size_t i = 0; i < in_size; i++)
  {
    lilv_instance_connect_port(fInstance, data.control_in_ports[i], &fInControls[i]);
  }

  for (std::size_t i = 0; i < out_size; i++)
  {
    lilv_instance_connect_port(
        fInstance, data.control_out_ports[i], &fOutControls[i]);
  }
}

void Model::reload()
{
  plugin = nullptr;

  auto path = m_effectPath;
  if (path.isEmpty())
    return;

  auto& app_plug
      = score::AppComponents().applicationPlugin<LV2::ApplicationPlugin>();
  hostContext = &app_plug.lv2_host_context;
  auto& world = app_plug.lilv;

  if (auto plug = find_lv2_plugin(world, path))
  {
    plugin = plug->me;
    effectContext.plugin.me = *plug;
    readPlugin();
    QString name = plug->get_name().as_string();
    if (name.isEmpty())
      name = path.split("/").back();
    metadata().setName(name);
    metadata().setLabel(name);
  }
}
}

static const void*
get_port_value(const char* port_symbol,
               void*       user_data,
               uint32_t*   size,
               uint32_t*   type)
{
  return nullptr;
  /*
  TestContext* ctx = (TestContext*)user_data;

  if (!strcmp(port_symbol, "input")) {
    *size = sizeof(float);
    *type = ctx->atom_Float;
    return &ctx->in;
  } else if (!strcmp(port_symbol, "output")) {
    *size = sizeof(float);
    *type = ctx->atom_Float;
    return &ctx->out;
  } else if (!strcmp(port_symbol, "control")) {
    *size = sizeof(float);
    *type = ctx->atom_Float;
    return &ctx->control;
  } else {
    fprintf(
      stderr, "error: get_port_value for nonexistent port `%s'\n", port_symbol);
    *size = *type = 0;
    return NULL;
  }
  */
}

#include <Process/Dataflow/PortFactory.hpp>

static LilvState*
state_from_instance(const LV2::Model& ctx)
{
  return lilv_state_new_from_instance(ctx.plugin,
                                      ctx.effectContext.instance,
                                      &ctx.hostContext->global->map,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      LV2_STATE_IS_PORTABLE,
                                      nullptr);
}

static
QByteArray readLV2State(const LV2::Model& eff)
{
  auto& global = *eff.hostContext->global;
  const auto state = state_from_instance(eff);
  auto str = lilv_state_to_string(eff.hostContext->world.me, &global.map, &global.unmap, state, "urn:io.ossia:lv2state", nullptr);

  QByteArray b(str);
  lilv_state_free(state);
  free(str);
  return b;
}

static
void restoreLV2State(LV2::Model& eff, const QByteArray& str)
{
  auto& global = *eff.hostContext->global;
  const auto state = lilv_state_new_from_string(eff.hostContext->world.me, &global.map, str.constData());

  lilv_state_restore(state, eff.effectContext.instance, nullptr, nullptr, LV2_STATE_IS_PORTABLE, nullptr);

  lilv_state_free(state);
}

template<typename T>
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
      *this,
      components.interfaces<Process::PortFactoryList>(),
      inls,
      outls,
      &eff);

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
      *this,
      components.interfaces<Process::PortFactoryList>(),
      inls,
      outls,
      &eff);

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
  void operator()();
};
void on_start::operator()()
{
  auto p = self.lock();
  if (!p)
    return;
  auto nn = p->node;
  if (!nn)
    return;
  auto& node = *static_cast<lv2_node_t*>(nn.get());
  auto& proc = p->process().to_process_events;

  LV2::Message msg;
  while(proc.try_dequeue(msg)) {
    for(std::size_t k = 0; k < node.m_atom_ins.size(); ++k)
    {
      if(node.data.midi_in_ports[k] == int32_t(msg.index))
      {
        node.m_message_for_atom_ins[k].push_back(std::move(msg));
        break;
      }
    }
  }
}
void on_finish::operator()()
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
               auto& node = *static_cast<lv2_node_t*>(nn.get());

               for (std::size_t k = 0; k < node.data.control_out_ports.size(); k++)
               {
                 auto port = (uint32_t)node.data.control_out_ports[k];
                 float val = node.fOutControls[k];

                 auto cport = static_cast<Model&>(p->process()).control_out_map[port];
                 SCORE_ASSERT(cport);
                 cport->setValue(val);
               }
             });

  auto nn = p->node;
  if (!nn)
    return;
  auto& node = *static_cast<lv2_node_t*>(nn.get());

  for(std::size_t k = 0; k < node.data.midi_out_ports.size(); ++k)
  {
    int port_index = node.data.midi_out_ports[k];
    AtomBuffer& buf = node.m_atom_outs[k];

    LV2_ATOM_SEQUENCE_FOREACH(&buf.buf->atoms, ev)
    {
      p->writeAtomToUi(port_index, ev->body);
    }
  }
}

LV2EffectComponent::LV2EffectComponent(
    LV2::Model& proc,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, "LV2Component", parent}
{
}

void LV2EffectComponent::lazy_init()
{
  auto& ctx = system();
  LV2::Model& proc = process();

  if (!proc.effectContext.instance)
    return;

  auto& host
      = ctx.context().doc.app.applicationPlugin<LV2::ApplicationPlugin>();
  on_start os;
  on_finish of;
  os.self = std::dynamic_pointer_cast<LV2EffectComponent>(shared_from_this());
  of.self = os.self;

  auto node = ossia::make_node<LV2::lv2_node_t>(
      *ctx.execState,
      LV2::LV2Data{host.lv2_host_context, proc.effectContext},
      ctx.execState->sampleRate,
      os,
      of);

  for (std::size_t i = proc.m_controlInStart; i < proc.inlets().size(); i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    node->fInControls[i - proc.m_controlInStart]
        = ossia::convert<float>(inlet->value());
    auto inl = node->root_inputs()[i];

    auto& vp = *inl->target<ossia::value_port>();
    vp.type = inlet->value().get_type();
    vp.domain = inlet->domain().get();

    connect(
        inlet,
        &Process::ControlInlet::valueChanged,
        this,
        [this, inl](const ossia::value& v) {
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
    uint8_t* data
        = reinterpret_cast<uint8_t*>(ev.body.data() + sizeof(LV2_Atom));

    auto b = (const uint8_t*)body;
    for (uint32_t i = 0; i < size; i++)
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
