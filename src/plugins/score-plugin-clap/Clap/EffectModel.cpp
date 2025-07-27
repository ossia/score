#include "EffectModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Audio/Settings/Model.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/thread.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDebug>
#include <QThread>
#include <QTimer>

#include <Clap/ApplicationPlugin.hpp>
#include <Clap/Window.hpp>
#include <clap/all.h>
#include <clap/ext/state.h>

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
  auto tm = new QTimer{};
  tm->setInterval(period_ms);
  *timer_id = g_next_timer_id.fetch_add(1, std::memory_order_relaxed);
  QObject::connect(tm, &QTimer::timeout, &m, [&m, tid = *timer_id] {
    if(auto t = m.handle()->ext_timer_support)
      t->on_timer(m.handle()->plugin, tid);
  });
  m.timers.push_back({*timer_id, tm});
  tm->start();
  return true;
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

static constexpr clap_host_state_t host_state_ext
    = {.mark_dirty = [](const clap_host_t*) {
  qDebug(Q_FUNC_INFO);
  // TODO

  // FIXME likely we want to accumulate and serialize the state of the plugin
  // so that we can restore in case of a crash.
  // But plug-ins may spam things so we need some debounce.
}};
static constexpr clap_host_params_t host_params_ext
    = {.rescan = [](const clap_host_t* host, clap_param_rescan_flags flags) {
  // TODO
  qDebug(Q_FUNC_INFO);
}, .clear = [](const clap_host_t* host, clap_id param_id, clap_param_clear_flags flags) {
  // TODO
  qDebug(Q_FUNC_INFO);
}, .request_flush = [](const clap_host_t* host) {
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  if(m.executing())
    return;

  auto plugin = m.handle()->plugin;
  auto params
      = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
  if(!params)
    return;
  if(!params->flush)
    return;

  clap_input_events_t ip;
  ip.ctx = nullptr;
  ip.get = +[](const struct clap_input_events* list,
               uint32_t index) -> const clap_event_header_t* { return nullptr; };
  ip.size = +[](const struct clap_input_events* list) -> uint32_t { return 0; };

  clap_output_events_t op{
      .ctx = nullptr,
      .try_push = [](const struct clap_output_events* list,
                     const clap_event_header_t* event) { return false; }};

  params->flush(plugin, &ip, &op);
}};

static constexpr clap_host_gui_t host_gui_ext
    = {.resize_hints_changed = resize_hints_changed,
       .request_resize = request_resize,
       .request_show = request_show,
       .request_hide = request_hide,
       .closed = closed};

static constexpr clap_host_track_info_t host_track_info_ext
    = {.get = [](const clap_host_t* host, clap_track_info_t* info) -> bool {
  auto& m = *static_cast<Clap::Model*>(host->host_data);
  auto* parent = Scenario::closestParentInterval(&m);
  if(!parent)
    return false;
  info->flags = CLAP_TRACK_INFO_HAS_TRACK_NAME | CLAP_TRACK_INFO_HAS_TRACK_COLOR;
  auto& meta = parent->metadata();
  std::string name;
  if(!meta.getLabel().isEmpty())
    name = meta.getLabel().toStdString();
  else if(!meta.getName().isEmpty())
    name = meta.getName().toStdString();

  std::strncpy(info->name, name.c_str(), CLAP_NAME_SIZE - 1);
  if(name.size() < CLAP_NAME_SIZE)
    info->name[name.size()] = 0;
  else
    info->name[CLAP_NAME_SIZE - 1] = 0;

  QRgb col = meta.getColor().getBrush().color().rgba();
  info->color = clap_color_t{
      .alpha = (uint8_t)qAlpha(col),
      .red = (uint8_t)qRed(col),
      .green = (uint8_t)qGreen(col),
      .blue = (uint8_t)qBlue(col)};
  return true;
}};

static constexpr clap_host_event_registry_t host_event_registry_ext
    = {.query = [](const clap_host_t* host, const char* space_name,
                   uint16_t* space_id) -> bool {
  *space_id = UINT16_MAX;
  return false;
}};

static constexpr clap_host_latency_t host_latency_ext
    = {.changed = [](const clap_host_t* host) {
  // TODO
}};

static constexpr clap_host_log_t host_log_ext
    = {.log = [](const clap_host_t* host, clap_log_severity severity, const char* msg) {
  switch(severity)
  {
    case CLAP_LOG_DEBUG:
      qDebug() << msg;
      break;
    case CLAP_LOG_INFO:
      qInfo() << msg;
      break;
    case CLAP_LOG_WARNING:
      qWarning() << msg;
      break;
    case CLAP_LOG_HOST_MISBEHAVING:
    case CLAP_LOG_PLUGIN_MISBEHAVING:
    case CLAP_LOG_ERROR:
      qCritical() << msg;
      break;
    case CLAP_LOG_FATAL:
      qFatal("%s", msg);
      break;
  }
}};

static constexpr clap_host_note_name_t host_note_name_ext
    = {.changed = [](const clap_host_t* host) {
  // TODO
}};

static constexpr clap_host_preset_load_t host_preset_load_ext
    = {.on_error =
           [](const clap_host_t* host, uint32_t location_kind, const char* location,
              const char* load_key, int32_t os_error, const char* msg) {
  // TODO
},
       .loaded = [](const clap_host_t* host, uint32_t location_kind,
                    const char* location, const char* load_key) {
  // TODO
}};

static constexpr clap_host_remote_controls_t host_remote_control_ext
    = {.changed = [](const clap_host_t* host) {
  // TODO
}, .suggest_page = [](const clap_host_t* host, clap_id page_id) {
  // TODO
}};

static constexpr clap_host_tail_t host_tail_ext
    = {.changed = [](const clap_host_t* host) {
  // TODO
}};

static constexpr clap_host_thread_check_t host_thread_check_ext = {
    .is_main_thread = [](const clap_host_t* host) -> bool {
  return ossia::get_current_thread_type() == ossia::thread_type::Ui;
},
    .is_audio_thread = [](const clap_host_t* host) -> bool {
  return ossia::get_current_thread_type() == ossia::thread_type::Audio;
},
};

static constexpr clap_host_thread_pool host_thread_pool_ext
    = {.request_exec = [](const clap_host_t* host, uint32_t num_tasks) -> bool {
  // TODO
  return false;
}};

static constexpr clap_host_voice_info_t host_voice_info_ext
    = {.changed = [](const clap_host_t* host) {
  // TODO
}};

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
    if(strcmp(extension_id, CLAP_EXT_STATE) == 0)
      return &host_state_ext;
    if(strcmp(extension_id, CLAP_EXT_PARAMS) == 0)
      return &host_params_ext;
    if(strcmp(extension_id, CLAP_EXT_TIMER_SUPPORT) == 0)
      return &host_timer_ext;
    if(strcmp(extension_id, CLAP_EXT_POSIX_FD_SUPPORT) == 0)
      return &host_posix_fd_ext;
    if(strcmp(extension_id, CLAP_EXT_TRACK_INFO) == 0)
      return &host_track_info_ext;
    if(strcmp(extension_id, "clap.track-info.draft/1") == 0)
      return &host_track_info_ext;
    if(strcmp(extension_id, CLAP_EXT_EVENT_REGISTRY) == 0)
      return &host_event_registry_ext;
    if(strcmp(extension_id, CLAP_EXT_LOG) == 0)
      return &host_log_ext;
    if(strcmp(extension_id, CLAP_EXT_LATENCY) == 0)
      return &host_latency_ext;
    if(strcmp(extension_id, CLAP_EXT_NOTE_NAME) == 0)
      return &host_note_name_ext;
    if(strcmp(extension_id, CLAP_EXT_PRESET_LOAD) == 0)
      return &host_preset_load_ext;
    if(strcmp(extension_id, "clap.preset-load.draft/2") == 0)
      return &host_preset_load_ext;
    if(strcmp(extension_id, CLAP_EXT_REMOTE_CONTROLS) == 0)
      return &host_remote_control_ext;
    if(strcmp(extension_id, "clap.remote-controls.draft/2") == 0)
      return &host_remote_control_ext;
    if(strcmp(extension_id, CLAP_EXT_TAIL) == 0)
      return &host_tail_ext;
    if(strcmp(extension_id, CLAP_EXT_THREAD_CHECK) == 0)
      return &host_thread_check_ext;
    if(strcmp(extension_id, CLAP_EXT_THREAD_POOL) == 0)
      return &host_thread_pool_ext;
    if(strcmp(extension_id, CLAP_EXT_VOICE_INFO) == 0)
      return &host_voice_info_ext;
    return nullptr;
  };
  host.request_restart = [](const clap_host* host) {
    auto& m = *static_cast<Clap::Model*>(host->host_data);
    QMetaObject::invokeMethod(&m, [&m] {
      auto plug = m.handle()->plugin;
      plug->on_main_thread(plug);
    }, Qt::QueuedConnection);
  };
  host.request_process = [](const clap_host* host) {
    auto& m = *static_cast<Clap::Model*>(host->host_data);
    // unused in score
  };
  host.request_callback = [](const clap_host* host) {
    auto& m = *static_cast<Clap::Model*>(host->host_data);
    QMetaObject::invokeMethod(&m, [&m] {
      auto plug = m.handle()->plugin;
      plug->on_main_thread(plug);
    }, Qt::QueuedConnection);
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

static void loadCLAPState(const clap_plugin_t& plugin, const QByteArray& state)
{
  if(state.isEmpty())
    return;

  // Get state extension
  auto state_ext
      = (const clap_plugin_state_t*)plugin.get_extension(&plugin, CLAP_EXT_STATE);
  if(!state_ext)
    return;

  // Create input stream
  struct stream_context
  {
    const QByteArray* data;
    int64_t position;
  };

  stream_context ctx{&state, 0};

  clap_istream_t stream{};
  stream.ctx = &ctx;
  stream.read
      = [](const clap_istream_t* stream, void* buffer, uint64_t size) -> int64_t {
    auto* ctx = static_cast<stream_context*>(stream->ctx);
    int64_t remaining = ctx->data->size() - ctx->position;
    int64_t to_read = std::min(static_cast<int64_t>(size), remaining);

    if(to_read <= 0)
      return 0;

    std::memcpy(buffer, ctx->data->constData() + ctx->position, to_read);
    ctx->position += to_read;
    return to_read;
  };

  // Load plugin state
  state_ext->load(&plugin, &stream);
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
  createControls(false);
}

Model::Model(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::Model(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::Model(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::Model(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_unique<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::~Model()
{
  closeUI();

  if(m_plugin->plugin && m_plugin->activated)
  {
    m_plugin->plugin->deactivate(m_plugin->plugin);
  }

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

void PluginHandle::load(Model& context, QByteArray path, QByteArray id)
{
  // Load the library
#if defined(_WIN32)
  library = LoadLibraryA(path.data());
  if(!library)
    return;
  entry = (const clap_plugin_entry_t*)GetProcAddress((HMODULE)library, "clap_entry");
#else
  library = dlopen(path.data(), RTLD_LAZY);
  if(!library)
    return;
  entry = (const clap_plugin_entry_t*)dlsym(library, "clap_entry");
#endif

  if(!entry || !entry->init(path.data()))
    return;

  factory = (const clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  if(!factory)
    return;

  host.host_data = &context;
  plugin = factory->create_plugin(factory, &host, id.data());
  if(!plugin)
    return;

  if(!plugin->init(plugin))
  {
    plugin->destroy(plugin);
    plugin = nullptr;
    return;
  }

  // Juce calls it during init??
  ext_timer_support = (const clap_plugin_timer_support_t*)plugin->get_extension(
      plugin, CLAP_EXT_TIMER_SUPPORT);
  ext_params
      = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);

  desc = plugin->desc;
}

void Model::loadPlugin()
{
  if(m_pluginPath.isEmpty() || m_pluginId.isEmpty())
    return;

  connect(this, &Process::ProcessModel::startExecution, this, [this] {
    m_executing = true;
  });
  connect(this, &Process::ProcessModel::stopExecution, this, [this] {
    m_executing = false;
  });

  m_plugin->load(*this, m_pluginPath.toUtf8(), m_pluginId.toUtf8());

  if(m_plugin->plugin && !m_loadedState.isEmpty())
  {
    loadCLAPState(*m_plugin->plugin, m_loadedState);
  }

  auto& audio_stgs = score::AppContext().settings<Audio::Settings::Model>();

  m_plugin->activated = m_plugin->plugin->activate(
      m_plugin->plugin, audio_stgs.getRate(), 1, audio_stgs.getBufferSize());
}

void Model::setupControlInlet(
    const clap_plugin_params_t& params, const clap_param_info_t& info, int index,
    Process::ControlInlet* inlet)
{
  inlet->setName(QString::fromUtf8(info.name));
  inlet->hidden = true; //(info.flags & CLAP_PARAM_IS_HIDDEN) != 0;

  // Set default value, min, and max for the control inlet
  double val{};
  // FIXME if we load we do not want to change the GUI inlet value, instead we
  // want to reapply it to the state (so that an inlet with a LFO as input does not
  // change its value randomly on every load
  if(params.get_value && params.get_value(m_plugin->plugin, info.id, &val))
  {
    inlet->setValue(val);
  }
  else
  {
    inlet->setValue(info.default_value);
  }

  inlet->setDomain(ossia::make_domain(info.min_value, info.max_value));

  connect(
      inlet, &Process::ControlInlet::valueChanged, this,
      [&params, i = index, this](const ossia::value& v) {
    if(this->m_executing)
      return;
    auto plugin = this->handle()->plugin;
    SCORE_ASSERT(this->parameterInputs().size() > i);
    auto& param_info = this->parameterInputs()[i];
    double val = ossia::convert<double>(v);

    clap_event_param_value_t param_event{};
    param_event.header.size = sizeof(clap_event_param_value_t);
    param_event.header.time = 0; // Beginning of buffer for now
    param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    param_event.header.type = CLAP_EVENT_PARAM_VALUE;
    param_event.header.flags = 0;
    param_event.param_id = param_info.id;
    param_event.cookie = param_info.cookie;
    param_event.note_id = -1;
    param_event.port_index = -1;
    param_event.channel = -1;
    param_event.key = -1;
    param_event.value = val;

    clap_input_events_t ip;
    ip.ctx = &param_event;
    ip.get = +[](const struct clap_input_events* list,
                 uint32_t index) -> const clap_event_header_t* {
      if(index == 0)
      {
        return (const clap_event_header_t*)&list->ctx;
      }
      return nullptr;
    };
    ip.size = +[](const struct clap_input_events* list) -> uint32_t { return 1; };

    clap_output_events_t op{
        .ctx = nullptr,
        .try_push = [](const struct clap_output_events* list,
                       const clap_event_header_t* event) { return false; }};
    params.flush(plugin, &ip, &op);
  });
}

void Model::setupControlOutlet(
    const clap_plugin_params_t& params, const clap_param_info_t& info, int index,
    Process::ControlOutlet* port)
{
  port->setName(QString::fromUtf8(info.name));
  port->hidden = true; //(info.flags & CLAP_PARAM_IS_HIDDEN) != 0;

  // Set default value, min, and max for the control inlet
  double val{};
  // FIXME if we load we do not want to change the GUI inlet value, instead we
  // want to reapply it to the state (so that an inlet with a LFO as input does not
  // change its value randomly on every load
  if(params.get_value && params.get_value(m_plugin->plugin, info.id, &val))
  {
    port->setValue(val);
  }
  else
  {
    port->setValue(info.default_value);
  }

  port->setDomain(ossia::make_domain(info.min_value, info.max_value));
}

void Model::createControls(bool loading)
{
  if(!m_plugin->plugin)
    return;

  int cur_inlet = 0;
  int cur_outlet = 0;

  // Get audio ports extension
  auto audio_ports = (const clap_plugin_audio_ports_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_AUDIO_PORTS);
  m_supports64 = true;
  m_audio_ins.clear();
  m_audio_outs.clear();
  if(audio_ports)
  {
    auto input_count = audio_ports->count(m_plugin->plugin, true);
    for(uint32_t i = 0; i < input_count; ++i)
    {
      clap_audio_port_info_t info;
      if(audio_ports->get(m_plugin->plugin, i, true, &info))
      {
        m_supports64 &= (info.flags & CLAP_AUDIO_PORT_SUPPORTS_64BITS);

        if(!loading)
        {
          auto inlet
              = new Process::AudioInlet(Id<Process::Port>(getStrongId(m_inlets)), this);
          inlet->setName(QString::fromUtf8(info.name));
          m_inlets.push_back(inlet);
        }

        m_audio_ins.push_back(info);
        cur_inlet++;
      }
    }
    auto output_count = audio_ports->count(m_plugin->plugin, false);
    for(uint32_t i = 0; i < output_count; ++i)
    {
      clap_audio_port_info_t info;
      if(audio_ports->get(m_plugin->plugin, i, false, &info))
      {
        m_supports64 &= (info.flags & CLAP_AUDIO_PORT_SUPPORTS_64BITS);

        if(!loading)
        {
          auto outlet = new Process::AudioOutlet(
              Id<Process::Port>(getStrongId(m_outlets)), this);
          outlet->setName(QString::fromUtf8(info.name));
          if(info.flags & CLAP_AUDIO_PORT_IS_MAIN)
          {
            outlet->setPropagate(true);
          }
          m_outlets.push_back(outlet);
        }

        m_audio_outs.push_back(info);
        cur_outlet++;
      }
    }
  }

  // Get note ports extension
  m_midi_ins.clear();
  m_midi_outs.clear();
  auto note_ports = (const clap_plugin_note_ports_t*)m_plugin->plugin->get_extension(m_plugin->plugin, CLAP_EXT_NOTE_PORTS);
  if(note_ports)
  {
    uint32_t input_count = note_ports->count(m_plugin->plugin, true);
    for(uint32_t i = 0; i < input_count; ++i)
    {
      clap_note_port_info_t info;
      if(note_ports->get(m_plugin->plugin, i, true, &info))
      {
        if(!loading)
        {
          auto inlet
              = new Process::MidiInlet(Id<Process::Port>(getStrongId(m_inlets)), this);
          inlet->setName(QString::fromUtf8(info.name));
          m_inlets.push_back(inlet);
        }

        m_midi_ins.push_back(info);
        cur_inlet++;
      }
    }
    uint32_t output_count = note_ports->count(m_plugin->plugin, false);
    for(uint32_t i = 0; i < output_count; ++i)
    {
      clap_note_port_info_t info;
      if(note_ports->get(m_plugin->plugin, i, false, &info))
      {
        if(!loading)
        {
          auto outlet
              = new Process::MidiOutlet(Id<Process::Port>(getStrongId(m_outlets)), this);
          outlet->setName(QString::fromUtf8(info.name));
          m_outlets.push_back(outlet);
        }

        m_midi_outs.push_back(info);
        cur_outlet++;
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
        if(info.flags & CLAP_PARAM_IS_HIDDEN)
          continue;
        if(!(info.flags & CLAP_PARAM_IS_READONLY))
        {
          if(!loading)
          {
            Process::ControlInlet* inlet{};
            if(info.flags & CLAP_PARAM_IS_STEPPED)
              inlet = new Process::IntSlider(
                  Id<Process::Port>(getStrongId(m_inlets)), this);
            else
              inlet = new Process::FloatSlider(
                  Id<Process::Port>(getStrongId(m_inlets)), this);

            setupControlInlet(*params, info, i, inlet);

            m_inlets.push_back(inlet);
          }
          else
          {
            setupControlInlet(
                *params, info, i,
                qobject_cast<Process::ControlInlet*>(m_inlets[cur_inlet]));
          }

          // Store parameter info for executor
          m_parameters_ins.push_back(info);
          cur_inlet++;
        }
        else
        {
          if(!loading)
          {
            Process::ControlOutlet* port{};
            port
                = new Process::Bargraph(Id<Process::Port>(getStrongId(m_outlets)), this);

            setupControlOutlet(*params, info, i, port);

            m_outlets.push_back(port);
          }
          else
          {
            setupControlOutlet(
                *params, info, i,
                qobject_cast<Process::ControlOutlet*>(m_inlets[cur_outlet]));
          }

          // Store parameter info for executor
          m_parameters_outs.push_back(info);
          cur_outlet++;
        }
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

static QByteArray readCLAPState(const clap_plugin_t& plugin)
{
  QByteArray dat;
  
  // Get state extension
  auto state = (const clap_plugin_state_t*)plugin.get_extension(&plugin, CLAP_EXT_STATE);
  if(!state)
    return dat;
  
  // Create a stream to capture the state
  struct stream_context {
    QByteArray* data;
  };
  
  stream_context ctx{&dat};
  
  clap_ostream_t stream{};
  stream.ctx = &ctx;
  stream.write = [](const clap_ostream_t* stream, const void* buffer, uint64_t size) -> int64_t {
    auto* ctx = static_cast<stream_context*>(stream->ctx);
    auto old_size = ctx->data->size();
    ctx->data->resize(old_size + size);
    std::memcpy(ctx->data->data() + old_size, buffer, size);
    return size;
  };
  
  // Save plugin state
  if(!state->save(&plugin, &stream))
    dat.clear();
  
  return dat;
}

template <>
void DataStreamReader::read(const Clap::Model& proc)
{
  QByteArray state;
  if(proc.m_plugin && proc.m_plugin->plugin)
  {
    state = readCLAPState(*proc.m_plugin->plugin);
  }
  
  m_stream << proc.m_pluginPath << proc.m_pluginId << state;
  
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Clap::Model& proc)
{
  m_stream >> proc.m_pluginPath >> proc.m_pluginId >> proc.m_loadedState;

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Clap::Model& proc)
{
  obj["PluginPath"] = proc.m_pluginPath;
  obj["PluginId"] = proc.m_pluginId;

  // FIXME CLAP_STATE_CONTEXT_FOR_PROJECT
  // Save plugin state
  QByteArray state;
  if(proc.m_plugin && proc.m_plugin->plugin)
  {
    state = readCLAPState(*proc.m_plugin->plugin);
  }
  obj["State"] = state.toBase64();
  
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Clap::Model& proc)
{
  proc.m_pluginPath = obj["PluginPath"].toString();
  proc.m_pluginId = obj["PluginId"].toString();
  proc.m_loadedState = QByteArray::fromBase64(obj["State"].toByteArray());

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
