#include "LV2EffectModel.hpp"

#include <QUrl>
#include <QFile>
#include "lv2_atom_helpers.hpp"
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>

#include <ModernMIDI/midi_message.h>
#include <set>
#include <iostream>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <ossia/detail/math.hpp>
#include <score/tools/Todo.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/network/domain/domain.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>

#include <ossia/dataflow/execution_state.hpp>
// TODO rename this file

#include "LV2Context.hpp"
#include "LV2Node.hpp"

namespace Media
{
namespace LV2
{
LV2EffectModel::LV2EffectModel(
    TimeVal t,
    const QString& path,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  ProcessModel{t, id, "LV2Effect", parent},
  m_effectPath{path}
{
  reload();
}

QString LV2EffectModel::prettyName() const
{
  return metadata().getLabel();
}

void LV2EffectModel::readPlugin()
{
  auto& app_plug = score::AppComponents().applicationPlugin<Media::ApplicationPlugin>();
  auto& h = app_plug.lv2_host_context;
  std::vector<float> fInControls, fOutControls, fParamMin, fParamMax, fParamInit, fOtherControls;

  LV2Data data{h, effectContext};

  const std::size_t in_size = data.control_in_ports.size();
  const std::size_t out_size = data.control_out_ports.size();
  const std::size_t midi_in_size = data.midi_in_ports.size();
  const std::size_t midi_out_size = data.midi_out_ports.size();
  //const std::size_t cv_size = data.cv_ports.size();
  //const std::size_t other_size = data.control_other_ports.size();
  const std::size_t num_ports = data.effect.plugin.get_num_ports();

  qDebug() << "in\t" << in_size << "\n"
           << "out\t" << out_size << "\n"
           << "min\t" << midi_in_size << "\n"
           << "mout\t" << midi_out_size << "\n"
           << "np\t" << num_ports;


  fParamMin.resize(num_ports);
  fParamMax.resize(num_ports);
  fParamInit.resize(num_ports);
  data.effect.plugin.get_port_ranges_float(fParamMin.data(), fParamMax.data(), fParamInit.data());

  qDeleteAll(m_inlets);
  m_inlets.clear();
  qDeleteAll(m_outlets);
  m_outlets.clear();


  int in_id = 0;
  int out_id = 0;

  m_inlets.push_back(new Process::Inlet{Id<Process::Port>{in_id++}, this});
  m_inlets[0]->type = Process::PortType::Audio;
  m_outlets.push_back(new Process::Outlet{Id<Process::Port>{out_id++}, this});
  m_outlets[0]->type = Process::PortType::Audio;

  for(int port_id : data.control_in_ports)
  {
    auto port = new Process::ControlInlet{Id<Process::Port>{in_id++}, this};

    port->setDomain(State::Domain{ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    port->setValue(fParamInit[port_id]);

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();

    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_inlets.push_back(port);
  }
  for(int port_id : data.control_out_ports)
  {
    auto port = new Process::ControlOutlet{Id<Process::Port>{out_id++}, this};

    port->setDomain(State::Domain{ossia::make_domain(fParamMin[port_id], fParamMax[port_id])});
    port->setValue(fParamInit[port_id]);

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_outlets.push_back(port);
  }

  for(int port_id : data.midi_in_ports)
  {
    auto port = new Process::Inlet{Id<Process::Port>{in_id++}, this};
    port->type = Process::PortType::Midi;

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_inlets.push_back(port);
  }

  for(int port_id : data.midi_out_ports)
  {
    auto port = new Process::Outlet{Id<Process::Port>{in_id++}, this};
    port->type = Process::PortType::Midi;

    Lilv::Port p = data.effect.plugin.get_port_by_index(port_id);
    Lilv::Node n = p.get_name();
    port->setCustomData(QString::fromUtf8(n.as_string()));

    m_outlets.push_back(port);
  }
}

void LV2EffectModel::reload()
{
  plugin = nullptr;

  auto path = m_effectPath;
  if(path.isEmpty())
    return;

  bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
  if(isFile)
  {
    if(*path.rbegin() != '/')
      path.append('/');
  }

  auto& app_plug = score::AppComponents().applicationPlugin<Media::ApplicationPlugin>();
  auto& world = app_plug.lilv;

  auto plugs = world.get_all_plugins();
  auto it = plugs.begin();
  while(!plugs.is_end(it))
  {
    auto plug = plugs.get(it);
    if((isFile && QString(plug.get_bundle_uri().as_string()) == path) || (!isFile && QString(plug.get_name().as_string()) == path))
    {
      plugin = plug.me;
      metadata().setLabel(QString(plug.get_name().as_string()));
      effectContext.plugin.me = plug;
      readPlugin();
      return;
    }
    else if(!isFile && QString(plug.get_name().as_string()) == path)
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
void DataStreamReader::read(
    const Media::LV2::LV2EffectModel& eff)
{
  m_stream << eff.effect();
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    Media::LV2::LV2EffectModel& eff)
{
  m_stream >> eff.m_effectPath;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(
    const Media::LV2::LV2EffectModel& eff)
{
  obj["Effect"] = eff.effect();
}

template <>
void JSONObjectWriter::write(
    Media::LV2::LV2EffectModel& eff)
{
  eff.m_effectPath = obj["Effect"].toString();
}

Engine::Execution::LV2EffectComponent::LV2EffectComponent(
    Media::LV2::LV2EffectModel& proc,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ProcessComponent_T{proc, ctx, id, "LV2Component", parent}
{
  auto& host = ctx.context().doc.app.applicationPlugin<Media::ApplicationPlugin>();
  node = std::make_shared<Media::LV2::lv2_node>(
        Media::LV2::LV2Data{
          host.lv2_host_context,
          proc.effectContext},
        ctx.plugin.execState->sampleRate);
  m_ossia_process = std::make_shared<ossia::node_process>(node);
}
