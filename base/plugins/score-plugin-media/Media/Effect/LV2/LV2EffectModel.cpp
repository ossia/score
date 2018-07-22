#include "LV2EffectModel.hpp"

#include "lv2_atom_helpers.hpp"

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <Engine/Executor/DocumentPlugin.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <QFile>
#include <QUrl>
#include <cmath>
#include <iostream>
#include <memory>
#include <score/tools/Todo.hpp>
#include <set>
// TODO rename this file

#include "LV2Context.hpp"
#include "LV2Node.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::LV2::LV2EffectModel)
namespace Process
{

template <>
QString
EffectProcessFactory_T<Media::LV2::LV2EffectModel>::customConstructionData()
    const
{
  auto& world = score::AppComponents()
                    .applicationPlugin<Media::ApplicationPlugin>()
                    .lilv;

  auto plugs = world.get_all_plugins();

  QStringList items;

  auto it = plugs.begin();
  while (!plugs.is_end(it))
  {
    auto plug = plugs.get(it);
    items.push_back(plug.get_name().as_string());
    it = plugs.next(it);
  }

  return QInputDialog::getItem(
      nullptr, QObject::tr("Select a plug-in"),
      QObject::tr("Select a LV2 plug-in"), items, 0, false);
}
}

namespace Media
{
namespace LV2
{

LV2EffectModel::LV2EffectModel(
    TimeVal t,
    const QString& path,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : ProcessModel{t, id, "LV2Effect", parent}, m_effectPath{path}
{
  reload();
}

QString LV2EffectModel::prettyName() const
{
  return metadata().getLabel();
}

void LV2EffectModel::readPlugin()
{
  auto& app_plug
      = score::AppComponents().applicationPlugin<Media::ApplicationPlugin>();
  auto& h = app_plug.lv2_host_context;
  ossia::float_vector fInControls, fOutControls, fParamMin, fParamMax,
      fParamInit, fOtherControls;

  LV2Data data{h, effectContext};

  const std::size_t audio_in_size = data.audio_in_ports.size();
  const std::size_t audio_out_size = data.audio_out_ports.size();
  const std::size_t in_size = data.control_in_ports.size();
  const std::size_t out_size = data.control_out_ports.size();
  const std::size_t midi_in_size = data.midi_in_ports.size();
  const std::size_t midi_out_size = data.midi_out_ports.size();
  const std::size_t cv_size = data.cv_ports.size();
  const std::size_t other_size = data.control_other_ports.size();
  const std::size_t num_ports = data.effect.plugin.get_num_ports();

  fParamMin.resize(num_ports);
  fParamMax.resize(num_ports);
  fParamInit.resize(num_ports);
  data.effect.plugin.get_port_ranges_float(
      fParamMin.data(), fParamMax.data(), fParamInit.data());

  qDeleteAll(m_inlets);
  m_inlets.clear();
  qDeleteAll(m_outlets);
  m_outlets.clear();

  int in_id = 0;
  int out_id = 0;

  // AUDIO
  if(audio_in_size > 0)
  {
    m_inlets.push_back(new Process::Inlet{Id<Process::Port>{in_id++}, this});
    m_inlets[0]->type = Process::PortType::Audio;
  }

  if(audio_out_size > 0)
  {
    m_outlets.push_back(new Process::Outlet{Id<Process::Port>{out_id++}, this});
    m_outlets[0]->type = Process::PortType::Audio;
    m_outlets[0]->setPropagate(true);
  }

  // CV
  for (std::size_t i = 0; i < cv_size; i++)
  {
    m_inlets.push_back(new Process::Inlet{Id<Process::Port>{in_id++}, this});
    m_inlets.back()->type = Process::PortType::Audio;
  }

  // MIDI
  for (int port_id : data.midi_in_ports)
  {
    auto port = new Process::Inlet{Id<Process::Port>{in_id++}, this};
    port->type = Process::PortType::Midi;

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_inlets.push_back(port);
  }

  for (int port_id : data.midi_out_ports)
  {
    auto port = new Process::Outlet{Id<Process::Port>{out_id++}, this};
    port->type = Process::PortType::Midi;

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_outlets.push_back(port);
  }

  m_controlInStart = in_id;
  // CONTROL
  for (int port_id : data.control_in_ports)
  {
    auto port = new Process::ControlInlet{Id<Process::Port>{in_id++}, this};

    port->hidden = true;
    port->setDomain(State::Domain{
        ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    port->setValue(fParamInit[port_id]);

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();

    port->setCustomData(QString::fromUtf8(n.as_string()));
    control_map.insert({port_id, {port, false}});
    connect(
        port, &Process::ControlInlet::valueChanged, this,
        [this, port, port_id] (const ossia::value& v) {
      auto& writing = control_map[port_id].second;
      writing = true;
      float f = ossia::convert<float>(v);
      suil_instance_port_event(effectContext.ui_instance, port_id, sizeof(float), 0, &f);
      writing = false;
    });

    m_inlets.push_back(port);
  }

  m_controlOutStart = in_id;
  for (int port_id : data.control_out_ports)
  {
    auto port = new Process::ControlOutlet{Id<Process::Port>{out_id++}, this};

    port->setDomain(State::Domain{
        ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    port->setValue(fParamInit[port_id]);

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));
    control_out_map.insert({port_id, port});

    m_outlets.push_back(port);
  }


  effectContext.instance = lilv_plugin_instantiate(
      effectContext.plugin.me, app_plug.lv2_context->sampleRate,
      app_plug.lv2_host_context.features);

  effectContext.data.data_access
      = lilv_instance_get_descriptor(effectContext.instance)->extension_data;
}

void LV2EffectModel::reload()
{
  plugin = nullptr;

  auto path = m_effectPath;
  if (path.isEmpty())
    return;

  bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
  if (isFile)
  {
    if (*path.rbegin() != '/')
      path.append('/');
  }

  auto& app_plug
      = score::AppComponents().applicationPlugin<Media::ApplicationPlugin>();
  auto& world = app_plug.lilv;

  auto plugs = world.get_all_plugins();
  auto it = plugs.begin();
  while (!plugs.is_end(it))
  {
    auto plug = plugs.get(it);
    if ((isFile && QString(plug.get_bundle_uri().as_string()) == path)
        || (!isFile && QString(plug.get_name().as_string()) == path))
    {
      plugin = plug.me;
      metadata().setLabel(QString(plug.get_name().as_string()));
      effectContext.plugin.me = plug;
      readPlugin();
      return;
    }
    else if (!isFile && QString(plug.get_name().as_string()) == path)
    {
      plugin = plug.me;
      effectContext.plugin.me = plug;
      readPlugin();
      metadata().setLabel(QString(plug.get_name().as_string()));
      return;
    }
    else
    {
      it = plugs.next(it);
    }
  }
}



}
}

template <>
void DataStreamReader::read(const Media::LV2::LV2EffectModel& eff)
{
  m_stream << eff.effect();
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::LV2::LV2EffectModel& eff)
{
  m_stream >> eff.m_effectPath;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::LV2::LV2EffectModel& eff)
{
  obj["Effect"] = eff.effect();
}

template <>
void JSONObjectWriter::write(Media::LV2::LV2EffectModel& eff)
{
  eff.m_effectPath = obj["Effect"].toString();
}

namespace Media::LV2
{
struct on_finish
{
  std::weak_ptr<Engine::Execution::ProcessComponent> self;
  void operator()()
  {
    auto p = self.lock();
    if(!p)
      return;

    p->in_edit([s=self] {
      auto p = s.lock();

      if(!p)
        return;
      auto nn = p->node;
      if(!nn)
        return;
      auto& node = *static_cast<lv2_node<on_finish>*>(nn.get());

      for(int k = 0; k < node.data.control_out_ports.size(); k++)
      {
        auto port = node.data.control_out_ports[k];
        float val = node.fOutControls[k];

        auto cport = static_cast<LV2EffectModel&>(p->process()).control_out_map[port];
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
    Media::LV2::LV2EffectModel& proc,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "LV2Component", parent}
{
}

void LV2EffectComponent::init()
{
  auto& ctx = system();
  auto& proc = process();

  auto& host
      = ctx.context().doc.app.applicationPlugin<Media::ApplicationPlugin>();
  on_finish of;
  of.self = shared_from_this();

  auto node = std::make_shared<Media::LV2::lv2_node<on_finish>>(
      Media::LV2::LV2Data{host.lv2_host_context, proc.effectContext},
      ctx.plugin.execState->sampleRate,
      of);

  for (std::size_t i = proc.m_controlInStart; i < proc.inlets().size(); i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    node->fInControls[i - proc.m_controlInStart] = ossia::convert<float>(inlet->value());
    auto inl = node->inputs()[i];
    connect(
        inlet, &Process::ControlInlet::valueChanged, this,
        [this, inl](const ossia::value& v) {
          system().executionQueue.enqueue([inl, val = v]() mutable {
            inl->data.target<ossia::value_port>()->add_value(
                std::move(val), 0);
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
  auto& p = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  Media::LV2::Message ev;
  ev.index    = port_index;
  ev.protocol = p.lv2_host_context.atom_eventTransfer;
  ev.body.resize(sizeof(LV2_Atom) + size);

  {
    LV2_Atom* atom = reinterpret_cast<LV2_Atom*>(ev.body.data());
    atom->type = type;
    atom->size = size;
  }

  {
    uint8_t* data = reinterpret_cast<uint8_t*>(ev.body.data() + sizeof(LV2_Atom));

    auto b = (const uint8_t*) body;
    for(uint32_t i = 0; i < size; i++)
      data[i] = b[i];
  }

  process().plugin_events.enqueue(std::move(ev));
}

}
W_OBJECT_IMPL(Media::LV2::LV2EffectComponent)
