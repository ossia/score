#include "EffectModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QDebug>
#include <QTimer>

#include <Clap/ApplicationPlugin.hpp>
#include <Clap/Window.hpp>
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
// Host GUI callbacks
extern "C" {
static void CLAP_ABI resize_hints_changed(const clap_host_t* host)
{
  // Handle resize hints change if needed
}

static bool CLAP_ABI
request_resize(const clap_host_t* host, uint32_t width, uint32_t height)
{
  auto* window = static_cast<Clap::Model*>(host->host_data)->window;
  if(window)
  {
    window->resize(width, height);
    return true;
  }
  return false;
}

static bool CLAP_ABI request_show(const clap_host_t* host)
{
  auto* window = static_cast<Clap::Model*>(host->host_data)->window;
  if(window)
  {
    window->show();
    return true;
  }
  return false;
}

static bool CLAP_ABI request_hide(const clap_host_t* host)
{
  auto* window = static_cast<Clap::Model*>(host->host_data)->window;
  if(window)
  {
    window->hide();
    return true;
  }
  return false;
}

static void CLAP_ABI closed(const clap_host_t* host, bool was_destroyed)
{
  auto* window = static_cast<Clap::Model*>(host->host_data)->window;
  if(window && was_destroyed)
  {
    // The plugin GUI was destroyed, we need to clean up
    window->close();
  }
}

static std::atomic<uint32_t> g_next_timer_id{};
CLAP_ABI bool
register_timer(const clap_host_t* host, uint32_t period_ms, clap_id* timer_id)
{
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  if(m.timer_support)
  {
    auto tm = new QTimer{};
    tm->setInterval(period_ms);
    *timer_id = g_next_timer_id.fetch_add(1, std::memory_order_relaxed);
    QObject::connect(tm, &QTimer::timeout, &m, [&m, tid = *timer_id] {
      m.timer_support->on_timer(m.handle()->plugin, tid);
    });
    m.timers.push_back({*timer_id, tm});
    tm->start();
    return true;
  }
  else
  {
    return false;
  }
}

// Returns true on success.
// [main-thread]
CLAP_ABI bool unregister_timer(const clap_host_t* host, clap_id timer_id)
{
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  for(auto it = m.timers.begin(); it != m.timers.end(); ++it)
  {
    if(it->first == timer_id)
    {
      delete it->second;
      m.timers.erase(it);
      return true;
    }
  }
  return false;
}

// POSIX fd support host extension
CLAP_ABI bool register_fd(const clap_host_t* host, int fd, clap_posix_fd_flags_t flags)
{
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  
  // Check if fd is already registered
  auto it = m.fd_notifiers.find(fd);
  if(it != m.fd_notifiers.end())
  {
    qWarning() << "CLAP: Attempted to register already registered fd" << fd;
    return false;
  }
  
  // Create notifier structure
  auto notifiers = std::make_unique<Model::FdNotifiers>();
  
  // Set up read notifier
  if(flags & CLAP_POSIX_FD_READ)
  {
    qDebug("READ FD");
    notifiers->read = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
    QObject::connect(notifiers->read.get(), &QSocketNotifier::activated, &m, [&m, fd]() {
      if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
           m.handle()->plugin->get_extension(m.handle()->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
      {
        qDebug("R FD !!!");
        plugin_fd_ext->on_fd(m.handle()->plugin, fd, CLAP_POSIX_FD_READ);
      }
    });
    notifiers->read->setEnabled(true);
  }
  
  // Set up write notifier
  if(flags & CLAP_POSIX_FD_WRITE)
  {
    qDebug("W FD");
    notifiers->write = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write);
    QObject::connect(notifiers->write.get(), &QSocketNotifier::activated, &m, [&m, fd]() {
      if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
           m.handle()->plugin->get_extension(m.handle()->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
      {
        qDebug("W FD !!!");
        plugin_fd_ext->on_fd(m.handle()->plugin, fd, CLAP_POSIX_FD_WRITE);
      }
    });
    notifiers->write->setEnabled(true);
  }
  
  // Set up error/exception notifier
  if(flags & CLAP_POSIX_FD_ERROR)
  {
    qDebug("E FD");
    notifiers->error = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Exception);
    QObject::connect(notifiers->error.get(), &QSocketNotifier::activated, &m, [&m, fd]() {
      if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
           m.handle()->plugin->get_extension(m.handle()->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
      {
        qDebug("E FD !!!");
        plugin_fd_ext->on_fd(m.handle()->plugin, fd, CLAP_POSIX_FD_ERROR);
      }
    });
    notifiers->error->setEnabled(true);
  }

  // Store the notifiers
  m.fd_notifiers[fd] = std::move(notifiers);
  return true;
}

CLAP_ABI bool modify_fd(const clap_host_t* host, int fd, clap_posix_fd_flags_t flags)
{
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  
  auto it = m.fd_notifiers.find(fd);
  if(it == m.fd_notifiers.end())
  {
    qWarning() << "CLAP: Attempted to modify unregistered fd" << fd;
    return false;
  }
  
  auto& notifiers = it->second;
  
  // Update read notifier
  if(flags & CLAP_POSIX_FD_READ)
  {
    if(!notifiers->read)
    {
      notifiers->read = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
      QObject::connect(notifiers->read.get(), &QSocketNotifier::activated, &m, [&m, fd]() {
        if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
             m.handle()->plugin->get_extension(m.handle()->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
        {
          plugin_fd_ext->on_fd(m.handle()->plugin, fd, CLAP_POSIX_FD_READ);
        }
      });
    }
    notifiers->read->setEnabled(true);
  }
  else if(notifiers->read)
  {
    notifiers->read->setEnabled(false);
  }
  
  // Update write notifier
  if(flags & CLAP_POSIX_FD_WRITE)
  {
    if(!notifiers->write)
    {
      notifiers->write = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write);
      QObject::connect(notifiers->write.get(), &QSocketNotifier::activated, &m, [&m, fd]() {
        if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
             m.handle()->plugin->get_extension(m.handle()->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
        {
          plugin_fd_ext->on_fd(m.handle()->plugin, fd, CLAP_POSIX_FD_WRITE);
        }
      });
    }
    notifiers->write->setEnabled(true);
  }
  else if(notifiers->write)
  {
    notifiers->write->setEnabled(false);
  }
  
  // Update error notifier
  if(flags & CLAP_POSIX_FD_ERROR)
  {
    if(!notifiers->error)
    {
      notifiers->error = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Exception);
      QObject::connect(notifiers->error.get(), &QSocketNotifier::activated, &m, [&m, fd]() {
        if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
             m.handle()->plugin->get_extension(m.handle()->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
        {
          plugin_fd_ext->on_fd(m.handle()->plugin, fd, CLAP_POSIX_FD_ERROR);
        }
      });
    }
    notifiers->error->setEnabled(true);
  }
  else if(notifiers->error)
  {
    notifiers->error->setEnabled(false);
  }
  
  return true;
}

CLAP_ABI bool unregister_fd(const clap_host_t* host, int fd)
{
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  
  auto it = m.fd_notifiers.find(fd);
  if(it == m.fd_notifiers.end())
  {
    qWarning() << "CLAP: Attempted to unregister unregistered fd" << fd;
    return false;
  }
  
  // QSocketNotifier objects will be automatically destroyed by unique_ptr
  m.fd_notifiers.erase(it);
  return true;
}
}

static constexpr clap_host_timer_support_t host_timer_ext
    = {.register_timer = register_timer, .unregister_timer = unregister_timer};

static constexpr clap_host_posix_fd_support_t host_posix_fd_ext
    = {.register_fd = register_fd, .modify_fd = modify_fd, .unregister_fd = unregister_fd};

static constexpr clap_host_gui_t host_gui_ext
    = {.resize_hints_changed = resize_hints_changed,
       .request_resize = request_resize,
       .request_show = request_show,
       .request_hide = request_hide,
       .closed = closed};

PluginHandle::PluginHandle()
{
  host.clap_version = CLAP_VERSION;
  host.name = "ossia score";
  host.vendor = "ossia.io";
  host.url = "https://ossia.io";
  host.version = SCORE_TAG_NO_V;
  host.get_extension
      = [](const clap_host* host, const char* extension_id) -> const void* {
    if(strcmp(extension_id, CLAP_EXT_GUI) == 0)
      return &host_gui_ext;
    if(strcmp(extension_id, CLAP_EXT_TIMER_SUPPORT) == 0)
      return &host_timer_ext;
    if(strcmp(extension_id, CLAP_EXT_POSIX_FD_SUPPORT) == 0)
      return &host_posix_fd_ext;
    return nullptr;
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

Model::~Model()
{
  closeUI();
  
  // Clean up timers
  for(auto& [id, timer] : timers)
  {
    delete timer;
  }
  timers.clear();
  
  // Clean up fd notifiers (automatically handled by unique_ptr destructors)
  fd_notifiers.clear();
}

bool Model::hasExternalUI() const noexcept
{
  if(!m_plugin->plugin)
    return false;
    
  auto gui = (const clap_plugin_gui_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_GUI);
  return gui != nullptr;
}

void Model::closeUI() const
{
  if(this->externalUI)
    this->externalUI->close();
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

  m_plugin->host.host_data = this;
  m_plugin->plugin = m_plugin->factory->create_plugin(m_plugin->factory, &m_plugin->host, m_pluginId.toUtf8().data());
  if(!m_plugin->plugin)
    return;

  timer_support = (const clap_plugin_timer_support_t*)m_plugin->plugin->get_extension(
      m_plugin->plugin, CLAP_EXT_TIMER_SUPPORT);

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
  // Clean up timers
  for(auto& [id, timer] : timers)
  {
    delete timer;
  }
  timers.clear();
  
  // Clean up fd notifiers
  fd_notifiers.clear();
  
  // Reset plugin handle
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
  m_audio_inputs_info.clear();
  m_audio_outputs_info.clear();
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
        m_audio_inputs_info.push_back(info);
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
        m_audio_outputs_info.push_back(info);
      }
    }
  }

  // Get note ports extension
  m_midi_inputs_info.clear();
  m_midi_outputs_info.clear();
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
        m_midi_inputs_info.push_back(info);
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
        m_midi_outputs_info.push_back(info);
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
QString
Process::EffectProcessFactory_T<Clap::Model>::customConstructionData() const noexcept
{
  return "";
}
template <>
Process::Descriptor
Process::EffectProcessFactory_T<Clap::Model>::descriptor(QString txt) const noexcept
{
  Process::Descriptor d;

  auto t = txt.split(":::");
  if(t.size() != 2)
    return d;
  auto pluginPath = t[0];
  auto pluginId = t[1];

  auto& plug = score::GUIAppContext().guiApplicationPlugin<Clap::ApplicationPlugin>();
  for(const auto& plugin : plug.plugins())
  {
    if(plugin.path == pluginPath && plugin.id == pluginId)
    {
      d.author = plugin.vendor;
      d.prettyName = plugin.name;
      d.description = plugin.description;
      d.documentationLink = plugin.manual_url;
      if(d.documentationLink.isEmpty())
        d.documentationLink = plugin.url;
      if(d.documentationLink.isEmpty())
        d.documentationLink = plugin.support_url;
      d.tags = plugin.features;
      break;
    }
  }
  return d;
}
template <>
Process::Descriptor Process::EffectProcessFactory_T<Clap::Model>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  Process::Descriptor desc;
  return descriptor(d.effect());
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
