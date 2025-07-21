#include "EffectModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <QDebug>
#include <QTimer>

#include <clap/all.h>

#include <score_git_info.hpp>
#include <wobjectimpl.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

W_OBJECT_IMPL(Clap::Model)

namespace Clap
{
PluginHandle::PluginHandle()
{
  host.clap_version = CLAP_VERSION;
  host.host_data = this;
  host.name = "ossia score";
  host.vendor = "ossia.io";
  host.url = "https://ossia.io";
  host.version = SCORE_TAG_NO_V;
  host.get_extension
      = [](const clap_host* host, const char* extension_id) -> const void* {
    return nullptr; // TODO: implement host extensions
  };
  host.request_restart = [](const clap_host* host) {
    // TODO: handle restart request
    qDebug() << "request_restart ";
  };
  host.request_process = [](const clap_host* host) {
    // TODO: handle process request
    qDebug() << "request_process ";
  };
  host.request_callback = [](const clap_host* host) {
    // TODO: handle callback request
    qDebug() << "request_callback ";
  };
}

PluginHandle::~PluginHandle()
{
  if(plugin)
  {
    plugin->destroy(plugin);
    plugin = nullptr;
  }

  if(entry)
  {
    entry->deinit();
    entry = nullptr;
  }

  if(library)
  {
#if defined(_WIN32)
    FreeLibrary((HMODULE)library);
#else
    dlclose(library);
#endif
    library = nullptr;
  }
}

Model::Model(
    const TimeVal& duration, const QString& pluginId,
    const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "ClapProcess", parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  auto plug = pluginId.split(":::");
  SCORE_ASSERT(plug.size() == 2);
  m_pluginPath = plug[0];
  m_pluginId = plug[1];

  loadPlugin();
  createControls();
}

Model::Model(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
}

Model::Model(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
}

Model::Model(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
}

Model::Model(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
}

Model::~Model() = default;

bool Model::hasExternalUI() const noexcept
{
  if(!m_plugin->plugin)
    return false;
    
  auto gui = (const clap_plugin_gui_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_GUI);
  return gui != nullptr;
}

void Model::reload()
{
  unloadPlugin();
  loadPlugin();
  createControls();
}

void Model::loadPlugin()
{
  if(m_pluginPath.isEmpty() || m_pluginId.isEmpty())
    return;

  // Load the library
#if defined(_WIN32)
  m_plugin->library = LoadLibraryA(m_pluginPath.toLocal8Bit().data());
  if(!m_plugin->library)
    return;
  m_plugin->entry = (const clap_plugin_entry_t*)GetProcAddress((HMODULE)m_plugin->library, "clap_entry");
#else
  m_plugin->library = dlopen(m_pluginPath.toUtf8().data(), RTLD_LAZY);
  if(!m_plugin->library)
    return;
  m_plugin->entry = (const clap_plugin_entry_t*)dlsym(m_plugin->library, "clap_entry");
#endif

  if(!m_plugin->entry || !m_plugin->entry->init(m_pluginPath.toUtf8().data()))
    return;

  m_plugin->factory = (const clap_plugin_factory_t*)m_plugin->entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  if(!m_plugin->factory)
    return;

  m_plugin->plugin = m_plugin->factory->create_plugin(m_plugin->factory, &m_plugin->host, m_pluginId.toUtf8().data());
  if(!m_plugin->plugin)
    return;

  if(!m_plugin->plugin->init(m_plugin->plugin))
  {
    m_plugin->plugin->destroy(m_plugin->plugin);
    m_plugin->plugin = nullptr;
    return;
  }

  m_plugin->desc = m_plugin->plugin->desc;
}

void Model::unloadPlugin()
{
  m_plugin = std::make_unique<PluginHandle>();
}

void Model::createControls()
{
  // Clear existing inlets/outlets
  m_inlets.clear();
  m_outlets.clear();
  m_parameters.clear();

  if(!m_plugin->plugin)
    return;

  // Get audio ports extension
  auto audio_ports = (const clap_plugin_audio_ports_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_AUDIO_PORTS);
  m_supports64 = true;
  m_inputs_info.clear();
  m_outputs_info.clear();
  if(audio_ports)
  {
    auto input_count = audio_ports->count(m_plugin->plugin, true);
    for(uint32_t i = 0; i < input_count; ++i)
    {
      clap_audio_port_info_t info;
      if(audio_ports->get(m_plugin->plugin, i, true, &info))
      {
        m_supports64 &= (info.flags & CLAP_AUDIO_PORT_SUPPORTS_64BITS);
        m_inlets.push_back(
            new Process::AudioInlet(Id<Process::Port>(getStrongId(m_inlets)), this));
        m_inputs_info.push_back(info);
      }
    }
    auto output_count = audio_ports->count(m_plugin->plugin, false);
    for(uint32_t i = 0; i < output_count; ++i)
    {
      clap_audio_port_info_t info;
      if(audio_ports->get(m_plugin->plugin, i, false, &info))
      {
        m_supports64 &= (info.flags & CLAP_AUDIO_PORT_SUPPORTS_64BITS);
        auto out
            = new Process::AudioOutlet(Id<Process::Port>(getStrongId(m_outlets)), this);
        if(info.flags & CLAP_AUDIO_PORT_IS_MAIN)
        {
          out->setPropagate(true);
        }
        m_outlets.push_back(out);
        m_outputs_info.push_back(info);
      }
    }
  }

  // Get note ports extension
  auto note_ports = (const clap_plugin_note_ports_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_NOTE_PORTS);
  if(note_ports)
  {
    uint32_t input_count = note_ports->count(m_plugin->plugin, true);
    for(uint32_t i = 0; i < input_count; ++i)
    {
      clap_note_port_info_t info;
      if(note_ports->get(m_plugin->plugin, i, true, &info))
      {
        auto inlet = new Process::MidiInlet(Id<Process::Port>(getStrongId(m_inlets)), this);
        inlet->setName(QString::fromUtf8(info.name));
        m_inlets.push_back(inlet);
      }
    }
    uint32_t output_count = note_ports->count(m_plugin->plugin, false);
    for(uint32_t i = 0; i < output_count; ++i)
    {
      clap_note_port_info_t info;
      if(note_ports->get(m_plugin->plugin, i, false, &info))
      {
        auto outlet = new Process::MidiOutlet(Id<Process::Port>(getStrongId(m_outlets)), this);
        outlet->setName(QString::fromUtf8(info.name));
        m_outlets.push_back(outlet);
      }
    }
  }

  // Get params extension
  auto params = (const clap_plugin_params_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_PARAMS);
  if(params)
  {
    uint32_t param_count = params->count(m_plugin->plugin);
    for(uint32_t i = 0; i < param_count; ++i)
    {
      clap_param_info_t info;
      if(params->get_info(m_plugin->plugin, i, &info))
      {
        auto inlet = new Process::ControlInlet(Id<Process::Port>(getStrongId(m_inlets)), this);
        inlet->setName(QString::fromUtf8(info.name));
        inlet->hidden = true; //(info.flags & CLAP_PARAM_IS_HIDDEN) != 0;
        
        // Set default value, min, and max for the control inlet
        inlet->setValue(info.default_value);
        inlet->setDomain(ossia::make_domain(info.min_value, info.max_value));
        
        m_inlets.push_back(inlet);
        
        // Store parameter info for executor
        ParameterInfo param_info;
        param_info.param_id = info.id;
        param_info.min_value = info.min_value;
        param_info.max_value = info.max_value;
        param_info.default_value = info.default_value;
        m_parameters.push_back(param_info);
      }
    }
  }

  inletsChanged();
  outletsChanged();
}

Process::ProcessFlags Model::flags() const noexcept
{
  auto flags = Metadata<Process::ProcessFlags_k, Model>::get();
  return flags;
}
}

template <>
void DataStreamReader::read(const Clap::Model& proc)
{
  m_stream << proc.m_pluginPath << proc.m_pluginId;
}

template <>
void DataStreamWriter::write(Clap::Model& proc)
{
  m_stream >> proc.m_pluginPath >> proc.m_pluginId;
}

template <>
void JSONReader::read(const Clap::Model& proc)
{
  obj["PluginPath"] = proc.m_pluginPath;
  obj["PluginId"] = proc.m_pluginId;
}

template <>
void JSONWriter::write(Clap::Model& proc)
{
  proc.m_pluginPath = obj["PluginPath"].toString();
  proc.m_pluginId = obj["PluginId"].toString();
}
