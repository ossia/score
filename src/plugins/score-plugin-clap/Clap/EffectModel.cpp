#include "EffectModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Audio/Settings/Model.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/json.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDebug>
#include <QMenu>
#include <QThread>
#include <QTimer>

#include <Clap/ApplicationPlugin.hpp>
#include <Clap/Window.hpp>
#include <clap/all.h>
#include <clap/ext/state.h>
#include <clap/ext/preset-load.h>
#include <clap/factory/preset-discovery.h>

#include <score_git_info.hpp>
#include <wobjectimpl.h>

#include <clap/ext/context-menu.h>

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
static void resize_hints_changed(const clap_host_t* host)
{
  // Handle resize hints change if needed
}

static bool request_resize(const clap_host_t* host, uint32_t width, uint32_t height)
{
  Clap::Model* model = static_cast<Clap::PluginHandle*>(host->host_data)->model;
  if(!model)
    return false;
  auto* window = model->window;
  if(window)
  {
    window->resize(width, height);
    return true;
  }
  return false;
}

static bool request_show(const clap_host_t* host)
{
  Clap::Model* model = static_cast<Clap::PluginHandle*>(host->host_data)->model;
  if(!model)
    return false;
  auto* window = model->window;
  if(window)
  {
    window->show();
    return true;
  }
  return false;
}

static bool request_hide(const clap_host_t* host)
{
  Clap::Model* model = static_cast<Clap::PluginHandle*>(host->host_data)->model;
  if(!model)
    return false;
  auto* window = model->window;
  if(window)
  {
    window->hide();
    return true;
  }
  return false;
}

static void closed(const clap_host_t* host, bool was_destroyed)
{
  Clap::Model* model = static_cast<Clap::PluginHandle*>(host->host_data)->model;
  if(!model)
    return;
  auto* window = model->window;
  if(window && was_destroyed)
  {
    // The plugin GUI was destroyed, we need to clean up
    window->close();
  }
}

static std::atomic<uint32_t> g_next_timer_id{};
bool register_timer(const clap_host_t* host, uint32_t period_ms, clap_id* timer_id)
{
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return false;

  auto tm = new QTimer{};
  tm->setInterval(period_ms);
  *timer_id = g_next_timer_id.fetch_add(1, std::memory_order_relaxed);
  QObject::connect(
      tm, &QTimer::timeout, m.model,
      [h = std::weak_ptr{m.model->handle()}, tid = *timer_id] {
    if(auto hh = h.lock())
      if(auto t = hh->ext_timer_support)
        t->on_timer(hh->plugin, tid);
  });
  m.model->timers.push_back({*timer_id, tm});
  tm->start();
  return true;
}

// Returns true on success.
// [main-thread]
bool unregister_timer(const clap_host_t* host, clap_id timer_id)
{
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return false;
  for(auto it = m.model->timers.begin(); it != m.model->timers.end(); ++it)
  {
    if(it->first == timer_id)
    {
      delete it->second;
      m.model->timers.erase(it);
      return true;
    }
  }
  return false;
}

// POSIX fd support host extension
bool register_fd(const clap_host_t* host, int fd, clap_posix_fd_flags_t flags)
{
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return false;

  // Check if fd is already registered
  auto it = m.model->fd_notifiers.find(fd);
  if(it != m.model->fd_notifiers.end())
  {
    qWarning() << "CLAP: Attempted to register already registered fd" << fd;
    return false;
  }

  // Create notifier structure
  auto notifiers = std::make_unique<Model::FdNotifiers>();

  // Set up read notifier
  if(flags & CLAP_POSIX_FD_READ)
  {
    notifiers->read = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
    QObject::connect(
        notifiers->read.get(), &QSocketNotifier::activated, m.model,
        [handle = std::weak_ptr{m.model->handle()}, fd]() {
      if(auto h = handle.lock())
        if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
               h->plugin->get_extension(h->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
        {
          plugin_fd_ext->on_fd(h->plugin, fd, CLAP_POSIX_FD_READ);
        }
    });
    notifiers->read->setEnabled(true);
  }

  // Set up write notifier
  if(flags & CLAP_POSIX_FD_WRITE)
  {
    notifiers->write = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write);
    QObject::connect(
        notifiers->write.get(), &QSocketNotifier::activated, m.model,
        [handle = std::weak_ptr{m.model->handle()}, fd]() {
      if(auto h = handle.lock())
        if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
               h->plugin->get_extension(h->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
        {
          plugin_fd_ext->on_fd(h->plugin, fd, CLAP_POSIX_FD_WRITE);
        }
    });
    notifiers->write->setEnabled(true);
  }

  // Set up error/exception notifier
  if(flags & CLAP_POSIX_FD_ERROR)
  {
    notifiers->error = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Exception);
    QObject::connect(
        notifiers->error.get(), &QSocketNotifier::activated, m.model,
        [handle = std::weak_ptr{m.model->handle()}, fd]() {
      if(auto h = handle.lock())
        if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
               h->plugin->get_extension(h->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
        {
          plugin_fd_ext->on_fd(h->plugin, fd, CLAP_POSIX_FD_ERROR);
        }
    });
    notifiers->error->setEnabled(true);
  }

  // Store the notifiers
  m.model->fd_notifiers[fd] = std::move(notifiers);
  return true;
}

bool modify_fd(const clap_host_t* host, int fd, clap_posix_fd_flags_t flags)
{
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return false;

  auto it = m.model->fd_notifiers.find(fd);
  if(it == m.model->fd_notifiers.end())
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
      QObject::connect(
          notifiers->read.get(), &QSocketNotifier::activated, m.model,
          [handle = std::weak_ptr{m.model->handle()}, fd]() {
        if(auto h = handle.lock())
          if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
                 h->plugin->get_extension(h->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
          {
            plugin_fd_ext->on_fd(h->plugin, fd, CLAP_POSIX_FD_READ);
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
      QObject::connect(
          notifiers->write.get(), &QSocketNotifier::activated, m.model,
          [handle = std::weak_ptr{m.model->handle()}, fd]() {
        if(auto h = handle.lock())
          if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
                 h->plugin->get_extension(h->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
          {
            plugin_fd_ext->on_fd(h->plugin, fd, CLAP_POSIX_FD_WRITE);
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
      notifiers->error
          = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Exception);
      QObject::connect(
          notifiers->error.get(), &QSocketNotifier::activated, m.model,
          [handle = std::weak_ptr{m.model->handle()}, fd]() {
        if(auto h = handle.lock())
          if(auto* plugin_fd_ext = static_cast<const clap_plugin_posix_fd_support_t*>(
                 h->plugin->get_extension(h->plugin, CLAP_EXT_POSIX_FD_SUPPORT)))
          {
            plugin_fd_ext->on_fd(h->plugin, fd, CLAP_POSIX_FD_ERROR);
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

bool unregister_fd(const clap_host_t* host, int fd)
{
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return false;

  auto it = m.model->fd_notifiers.find(fd);
  if(it == m.model->fd_notifiers.end())
  {
    qWarning() << "CLAP: Attempted to unregister unregistered fd" << fd;
    return false;
  }

  // QSocketNotifier objects will be automatically destroyed by unique_ptr
  m.model->fd_notifiers.erase(it);
  return true;
}

static QByteArray readCLAPState(const clap_plugin_t& plugin)
{
  QByteArray dat;

  // Get state extension
  auto state = (const clap_plugin_state_t*)plugin.get_extension(&plugin, CLAP_EXT_STATE);
  if(!state)
    return dat;

  // Create a stream to capture the state
  struct stream_context
  {
    QByteArray* data;
  };

  stream_context ctx{&dat};

  clap_ostream_t stream{};
  stream.ctx = &ctx;
  stream.write
      = [](const clap_ostream_t* stream, const void* buffer, uint64_t size) -> int64_t {
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
}

static constexpr clap_host_timer_support_t host_timer_ext
    = {.register_timer = register_timer, .unregister_timer = unregister_timer};

static constexpr clap_host_posix_fd_support_t host_posix_fd_ext = {
    .register_fd = register_fd, .modify_fd = modify_fd, .unregister_fd = unregister_fd};

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
  qDebug(Q_FUNC_INFO);
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return;

  auto plugin = m.plugin;
  auto params
      = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
  if(!params)
    return;
  if(!params->flush)
    return;

  if(!m.model->executing())
    m.model->flushFromPluginToHost();
}};

static constexpr clap_host_gui_t host_gui_ext
    = {.resize_hints_changed = resize_hints_changed,
       .request_resize = request_resize,
       .request_show = request_show,
       .request_hide = request_hide,
       .closed = closed};

static constexpr clap_host_track_info_t host_track_info_ext
    = {.get = [](const clap_host_t* host, clap_track_info_t* info) -> bool {
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  return false;
  auto* parent = Scenario::closestParentInterval(m.model);
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
  qWarning() << "CLAP preset load error:" << (msg ? msg : "Unknown error")
             << "Location:" << (location ? location : "null")  
             << "Load key:" << (load_key ? load_key : "null")
             << "OS error:" << os_error;
},
       .loaded = [](const clap_host_t* host, uint32_t location_kind,
                    const char* location, const char* load_key) {
  qDebug() << "CLAP preset loaded successfully:"
           << "Location:" << (location ? location : "null")
           << "Load key:" << (load_key ? load_key : "null");
}};

static constexpr clap_host_remote_controls_t host_remote_control_ext
    = {.changed = [](const clap_host_t* host) {
  qDebug(Q_FUNC_INFO);
  // TODO
}, .suggest_page = [](const clap_host_t* host, clap_id page_id) {
  qDebug(Q_FUNC_INFO);
  // TODO
}};

static constexpr clap_host_tail_t host_tail_ext
    = {.changed = [](const clap_host_t* host) {
  qDebug(Q_FUNC_INFO);
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
  qDebug(Q_FUNC_INFO);
  // TODO
  return false;
}};

static constexpr clap_host_voice_info_t host_voice_info_ext
    = {.changed = [](const clap_host_t* host) {
  qDebug(Q_FUNC_INFO);
  // TODO
}};

// Context menu builder implementation
struct context_menu_builder_impl
{
  QMenu* menu;
  QMenu* root_menu;
  const clap_plugin_t* plugin;
  const clap_plugin_context_menu_t* plugin_context_menu;
  const clap_context_menu_target_t* target;

  context_menu_builder_impl(
      QMenu* m, const clap_plugin_t* p, const clap_plugin_context_menu_t* pcm,
      const clap_context_menu_target_t* t)
      : menu(m)
      , root_menu(m)
      , plugin(p)
      , plugin_context_menu(pcm)
      , target(t)
  {
  }
};

static bool context_menu_add_item(
    const clap_context_menu_builder_t* builder, clap_context_menu_item_kind_t item_kind,
    const void* item_data)
{
  auto* impl = static_cast<context_menu_builder_impl*>(builder->ctx);
  if(!impl || !impl->menu)
    return false;

  switch(item_kind)
  {
    case CLAP_CONTEXT_MENU_ITEM_ENTRY: {
      auto* entry = static_cast<const clap_context_menu_entry_t*>(item_data);
      auto* action = impl->menu->addAction(QString::fromUtf8(entry->label));
      action->setEnabled(entry->is_enabled);

      clap_id action_id = entry->action_id;
      QObject::connect(action, &QAction::triggered, [action_id, builder]() {
        auto* impl = static_cast<context_menu_builder_impl*>(builder->ctx);
        if(impl && impl->plugin_context_menu)
        {
          impl->plugin_context_menu->perform(impl->plugin, impl->target, action_id);
        }
      });
      return true;
    }

    case CLAP_CONTEXT_MENU_ITEM_CHECK_ENTRY: {
      auto* entry = static_cast<const clap_context_menu_check_entry_t*>(item_data);
      auto* action = impl->menu->addAction(QString::fromUtf8(entry->label));
      action->setEnabled(entry->is_enabled);
      action->setCheckable(true);
      action->setChecked(entry->is_checked);

      clap_id action_id = entry->action_id;
      QObject::connect(action, &QAction::triggered, [action_id, builder]() {
        auto* impl = static_cast<context_menu_builder_impl*>(builder->ctx);
        if(impl && impl->plugin_context_menu)
        {
          impl->plugin_context_menu->perform(impl->plugin, impl->target, action_id);
        }
      });
      return true;
    }

    case CLAP_CONTEXT_MENU_ITEM_SEPARATOR: {
      impl->menu->addSeparator();
      return true;
    }

    case CLAP_CONTEXT_MENU_ITEM_BEGIN_SUBMENU: {
      auto* submenu_data = static_cast<const clap_context_menu_submenu_t*>(item_data);
      auto* submenu = impl->menu->addMenu(QString::fromUtf8(submenu_data->label));
      submenu->setEnabled(submenu_data->is_enabled);
      impl->menu = submenu; // Switch context to submenu
      return true;
    }

    case CLAP_CONTEXT_MENU_ITEM_END_SUBMENU: {
      // Switch back to parent menu
      if(impl->menu != impl->root_menu)
      {
        // Find parent menu by traversing up the object hierarchy
        QObject* parent = impl->menu->parent();
        while(parent && !qobject_cast<QMenu*>(parent))
          parent = parent->parent();
        if(parent)
          impl->menu = static_cast<QMenu*>(parent);
        else
          impl->menu = impl->root_menu;
      }
      return true;
    }

    case CLAP_CONTEXT_MENU_ITEM_TITLE: {
      auto* title = static_cast<const clap_context_menu_item_title_t*>(item_data);
      auto* action = impl->menu->addAction(QString::fromUtf8(title->title));
      action->setEnabled(false); // Titles are not clickable
      auto font = action->font();
      font.setBold(true);
      action->setFont(font);
      return true;
    }
  }
  return false;
}

static bool context_menu_supports(
    const clap_context_menu_builder_t* builder, clap_context_menu_item_kind_t item_kind)
{
  // We support all standard context menu item types
  return item_kind == CLAP_CONTEXT_MENU_ITEM_ENTRY
         || item_kind == CLAP_CONTEXT_MENU_ITEM_CHECK_ENTRY
         || item_kind == CLAP_CONTEXT_MENU_ITEM_SEPARATOR
         || item_kind == CLAP_CONTEXT_MENU_ITEM_BEGIN_SUBMENU
         || item_kind == CLAP_CONTEXT_MENU_ITEM_END_SUBMENU
         || item_kind == CLAP_CONTEXT_MENU_ITEM_TITLE;
}

static constexpr clap_host_context_menu_t host_context_menu_ext = {
    .populate = [](const clap_host_t* host, const clap_context_menu_target_t* target,
                   const clap_context_menu_builder_t* builder) -> bool { return true; },

    .perform = [](const clap_host_t* host, const clap_context_menu_target_t* target,
                  clap_id action_id) -> bool { return false; },

    .can_popup = [](const clap_host_t* host) -> bool { return true; },

    .popup = [](const clap_host_t* host, const clap_context_menu_target_t* target,
                int32_t screen_index, int32_t x, int32_t y) -> bool {
  auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
  if(!m.model)
    return false;
  auto& model = *m.model;
  // Get the plugin's context menu extension
  auto* plugin_context_menu = static_cast<const clap_plugin_context_menu_t*>(
      m.plugin->get_extension(m.plugin, CLAP_EXT_CONTEXT_MENU));

  if(!plugin_context_menu)
    return false;

  // Create a QMenu
  auto menu = std::make_unique<QMenu>();

  // Create builder context
  context_menu_builder_impl builder_ctx(
      menu.get(), m.plugin, plugin_context_menu, target);
  clap_context_menu_builder_t builder
      = {.ctx = &builder_ctx,
         .add_item = context_menu_add_item,
         .supports = context_menu_supports};

  // Let the plugin populate the menu
  if(!plugin_context_menu->populate(m.plugin, target, &builder))
    return false;

  // Show the menu at the specified position
  QPoint pos(x, y);
  menu->exec(pos);

  return true;
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
    if(strcmp(extension_id, CLAP_EXT_CONTEXT_MENU) == 0)
      return &host_context_menu_ext;
    if(strcmp(extension_id, "clap.context-menu.draft/0") == 0)
      return &host_context_menu_ext;
    if(strcmp(extension_id, CLAP_EXT_PRESET_LOAD) == 0)
      return &host_preset_load_ext;
    if(strcmp(extension_id, "clap.preset-load.draft/2") == 0)
      return &host_preset_load_ext;
    return nullptr;
  };
  host.request_restart = [](const clap_host* host) {
    auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
    if(!m.model)
      return;
    QMetaObject::invokeMethod(m.model, [handle = std::weak_ptr{m.model->handle()}] {
      if(auto h = handle.lock())
      {
        // FIXME implement restart
        auto plug = h->plugin;
        plug->on_main_thread(plug);
      }
    }, Qt::QueuedConnection);
  };
  host.request_process = [](const clap_host* host) {
    auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
    if(!m.model)
      return;
    // unused in score
  };
  host.request_callback = [](const clap_host* host) {
    auto& m = *static_cast<Clap::PluginHandle*>(host->host_data);
    if(!m.model)
      return;
    QMetaObject::invokeMethod(m.model, [handle = std::weak_ptr{m.model->handle()}] {
      if(auto h = handle.lock())
      {
        auto plug = h->plugin;
        plug->on_main_thread(plug);
      }
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
    , m_plugin{std::make_shared<PluginHandle>()}
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
    , m_plugin{std::make_shared<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::Model(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_shared<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::Model(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_shared<PluginHandle>()}
{
  vis.writeTo(*this);
  loadPlugin();
  createControls(true);
}

Model::Model(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
    , m_plugin{std::make_shared<PluginHandle>()}
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
    m_plugin->activated = false;
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

  auto gui = (const clap_plugin_gui_t*)m_plugin->plugin->get_extension(
      m_plugin->plugin, CLAP_EXT_GUI);
  return gui != nullptr;
}

void Model::closeUI() const
{
  if(this->externalUI)
    this->externalUI->close();
}

void PluginHandle::load(Model& context, QByteArray path, QByteArray id)
{
  this->model = &context;

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

  host.host_data = this;
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

  m_plugin->load(*this, m_pluginPath.toUtf8(), m_pluginId.toUtf8());

  if(m_plugin->plugin && !m_loadedState.isEmpty())
  {
    loadCLAPState(*m_plugin->plugin, m_loadedState);
  }

  m_plugin->activated = false;

  // Connect parameter value feedback from executor to GUI
  auto& ctx = score::IDocument::documentContext(*this);
  auto c = connect(&ctx.coarseUpdateTimer, &QTimer::timeout, this, [this] {
    if(auto plugin = handle()->plugin)
    {
      // Get parameter extension to read current values
      auto params
          = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
      if(params)
      {
        std::size_t control_idx = 0;
        for(auto* inlet : inlets())
        {
          if(auto* control = qobject_cast<Process::ControlInlet*>(inlet))
          {
            if(control_idx < m_plugin->m_parameters_ins.size())
            {
              const auto& param_info = m_plugin->m_parameters_ins[control_idx];
              double current_value = 0.0;

              // Read current parameter value from plugin
              if(params->get_value(plugin, param_info.id, &current_value))
              {
                currentlyReadingValues = true;
                control->setValue(current_value);
                currentlyReadingValues = false;
              }
            }
            control_idx++;
          }
        }
      }
    }
  });

  // Connect the restart signal
  connect(this, &Process::ProcessModel::resetExecution, this, [this] {
    if(m_plugin->plugin && m_plugin->activated)
    {
      m_plugin->plugin->deactivate(m_plugin->plugin);
      m_plugin->activated = false;
    }
  }, Qt::QueuedConnection);
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
      [this, i = index](const ossia::value& v) {
    if(executing())
      return;
    SCORE_ASSERT(this->parameterInputs().size() > i);
    auto& param_info = this->parameterInputs()[i];
    double val = ossia::convert<double>(v);

    auto plugin = m_plugin->plugin;
    auto params
        = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    if(!params)
      return;
    clap_event_param_value_t param_event{};
    param_event.header.size = sizeof(clap_event_param_value_t);
    param_event.header.time = 0; // Beginning of buffer for now
    param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    param_event.header.type = CLAP_EVENT_PARAM_VALUE;
    param_event.header.flags = CLAP_EVENT_IS_LIVE;
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
        .ctx = this,
        .try_push = [](const struct clap_output_events* list,
                       const clap_event_header_t* event) { return false; }};

    params->flush(plugin, &ip, &op);
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
  auto audio_ports = (const clap_plugin_audio_ports_t*)m_plugin->plugin->get_extension(
      m_plugin->plugin, CLAP_EXT_AUDIO_PORTS);
  m_supports64 = true;
  m_plugin->m_audio_ins.clear();
  m_plugin->m_audio_outs.clear();
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

        m_plugin->m_audio_ins.push_back(info);
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

        m_plugin->m_audio_outs.push_back(info);
        cur_outlet++;
      }
    }
  }

  // Get note ports extension
  m_plugin->m_midi_ins.clear();
  m_plugin->m_midi_outs.clear();
  auto note_ports = (const clap_plugin_note_ports_t*)m_plugin->plugin->get_extension(
      m_plugin->plugin, CLAP_EXT_NOTE_PORTS);
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

        m_plugin->m_midi_ins.push_back(info);
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

        m_plugin->m_midi_outs.push_back(info);
        cur_outlet++;
      }
    }
  }

  // Get params extension
  auto params = (const clap_plugin_params_t*)m_plugin->plugin->get_extension(
      m_plugin->plugin, CLAP_EXT_PARAMS);
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
          m_plugin->m_parameters_ins.push_back(info);
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
                qobject_cast<Process::ControlOutlet*>(m_outlets[cur_outlet]));
          }

          // Store parameter info for executor
          m_plugin->m_parameters_outs.push_back(info);
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

void Model::loadPreset(const Process::Preset& preset)
{
  if(!m_plugin || !m_plugin->plugin)
    return;

  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();

  // Try to load using CLAP state (preferred method)
  if(auto it = obj.FindMember("State"); it != obj.MemberEnd())
  {
    QByteArray data = QByteArray::fromBase64(JsonValue{it->value}.toByteArray());
    if(!data.isEmpty())
    {
      // Get state extension and load the state
      auto state = (const clap_plugin_state_t*)m_plugin->plugin->get_extension(
          m_plugin->plugin, CLAP_EXT_STATE);
      if(state)
      {
        struct stream_context
        {
          const char* data;
          size_t size;
          size_t pos;
        };

        stream_context ctx{data.constData(), static_cast<size_t>(data.size()), 0};

        clap_istream_t stream{};
        stream.ctx = &ctx;
        stream.read
            = [](const clap_istream_t* stream, void* buffer, uint64_t size) -> int64_t {
          auto* ctx = static_cast<stream_context*>(stream->ctx);
          if(ctx->pos >= ctx->size)
            return 0;

          uint64_t to_read = std::min(int64_t(size), int64_t(ctx->size - ctx->pos));
          std::memcpy(buffer, ctx->data + ctx->pos, to_read);
          ctx->pos += to_read;
          return static_cast<int64_t>(to_read);
        };

        // Load the state
        if(state->load(m_plugin->plugin, &stream))
        {
          // Update parameters after loading state
          auto params = (const clap_plugin_params_t*)m_plugin->plugin->get_extension(
              m_plugin->plugin, CLAP_EXT_PARAMS);
          if(params)
          {
            for(auto& inlet : m_inlets)
            {
              if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet))
              {
                // Find parameter by ID and update inlet value
                for(const auto& param_info : m_plugin->m_parameters_ins)
                {
                  if(param_info.id == ctl->id().val())
                  {
                    double value;
                    if(params->get_value(m_plugin->plugin, param_info.id, &value))
                    {
                      ctl->setValue(value);
                    }
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  // Try preset load extension (for file-based and builtin presets)
  else if(auto loc_it = obj.FindMember("PresetLocation"); loc_it != obj.MemberEnd())
  {
    auto preset_load = (const clap_plugin_preset_load_t*)m_plugin->plugin->get_extension(
        m_plugin->plugin, CLAP_EXT_PRESET_LOAD);
    if(!preset_load)
      preset_load = (const clap_plugin_preset_load_t*)m_plugin->plugin->get_extension(
          m_plugin->plugin, "clap.preset-load.draft/2");
    if(preset_load)
    {
      QString location = JsonValue{loc_it->value}.toString();
      QString load_key;
      uint32_t location_kind = CLAP_PRESET_DISCOVERY_LOCATION_FILE; // Default to file

      // Check for location kind (for builtin presets)
      if(auto kind_it = obj.FindMember("LocationKind"); kind_it != obj.MemberEnd())
      {
        location_kind = static_cast<uint32_t>(JsonValue{kind_it->value}.toInt());
      }

      if(auto key_it = obj.FindMember("LoadKey"); key_it != obj.MemberEnd())
      {
        load_key = JsonValue{key_it->value}.toString();
      }

      // For builtin presets, location should be null/empty
      const char* location_str = "";
      if(location_kind == CLAP_PRESET_DISCOVERY_LOCATION_FILE && !location.isEmpty())
      {
        location_str = location.toUtf8().constData();
      }

      preset_load->from_location(
          m_plugin->plugin, location_kind, location_str,
          load_key.isEmpty() ? "" : load_key.toUtf8().constData());
    }
  }
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();
  p.key.effect = this->effect();

  if(!m_plugin || !m_plugin->plugin)
  {
    p.data = "{}";
    return p;
  }

  // Save using CLAP state extension
  QByteArray data = readCLAPState(*m_plugin->plugin);

  JSONReader r;
  r.stream.StartObject();
  r.obj["State"] = data.toBase64();
  r.stream.EndObject();
  p.data = r.toByteArray();
  return p;
}

std::vector<Process::Preset> Model::builtinPresets() const noexcept
{
  std::vector<Process::Preset> presets;

  if(!m_plugin || !m_plugin->plugin || !m_plugin->entry)
    return presets;

  // Get the preset discovery factory from the plugin entry
  auto preset_discovery_factory = static_cast<const clap_preset_discovery_factory_t*>(
      m_plugin->entry->get_factory(CLAP_PRESET_DISCOVERY_FACTORY_ID));
  
  if(!preset_discovery_factory)
  {
    // Try the compatibility ID as fallback
    preset_discovery_factory = static_cast<const clap_preset_discovery_factory_t*>(
        m_plugin->entry->get_factory("clap.preset-discovery-factory/draft-2"));
  }

  if(!preset_discovery_factory)
    return presets;

  // Create a temporary indexer to receive preset metadata
  struct preset_metadata_collector
  {
    std::vector<Process::Preset>* target_presets;
    QString plugin_name;
    UuidKey<Process::ProcessModel> process_key;
    QString effect_name;
    std::vector<const clap_preset_discovery_location_t*> locations;
  };

  preset_metadata_collector collector{&presets, this->prettyName(), this->concreteKey(), this->effect()};

  clap_preset_discovery_indexer_t indexer{};
  indexer.clap_version = CLAP_VERSION;
  indexer.name = "ossia score";
  indexer.vendor = "ossia.io";
  indexer.url = "https://ossia.io";
  indexer.version = SCORE_TAG_NO_V;
  indexer.indexer_data = &collector;

  // Implement required indexer callbacks
  indexer.declare_filetype = [](const clap_preset_discovery_indexer_t* indexer,
                                const clap_preset_discovery_filetype_t* filetype) -> bool {
    return true; // Accept all filetypes for builtin presets
  };

  indexer.declare_location
      = [](const clap_preset_discovery_indexer_t* indexer,
           const clap_preset_discovery_location_t* location) -> bool {
    auto& self = *(preset_metadata_collector*)indexer->indexer_data;
    self.locations.push_back(location);
    return true; // Accept all locations for builtin presets
  };

  indexer.declare_soundpack = [](const clap_preset_discovery_indexer_t* indexer,
                                 const clap_preset_discovery_soundpack_t* soundpack) -> bool {
    return true; // Accept all soundpacks
  };

  indexer.get_extension = [](const clap_preset_discovery_indexer_t* indexer,
                             const char* extension_id) -> const void* {
    return nullptr; // No extensions needed for basic preset discovery
  };

  // Create metadata receiver to collect preset information
  clap_preset_discovery_metadata_receiver_t metadata_receiver{};
  metadata_receiver.receiver_data = &collector;

  metadata_receiver.on_error = [](const clap_preset_discovery_metadata_receiver_t* receiver,
                                  int32_t os_error,
                                  const char* error_message) {
    qWarning() << "CLAP preset discovery error:" << (error_message ? error_message : "Unknown error");
  };

  metadata_receiver.begin_preset = [](const clap_preset_discovery_metadata_receiver_t* receiver,
                                      const char* name,
                                      const char* load_key) -> bool {
    auto* collector = static_cast<preset_metadata_collector*>(receiver->receiver_data);
    
    Process::Preset preset;
    preset.name = QString::fromUtf8(name ? name : "Unnamed Preset");
    preset.key.key = collector->process_key;
    preset.key.effect = collector->effect_name;

    // Store preset location information for loading later
    JSONReader r;
    r.stream.StartObject();
    r.obj["LocationKind"] = static_cast<int>(CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN);
    r.obj["PresetLocation"] = QString{}; // Empty for builtin presets
    if(load_key)
      r.obj["LoadKey"] = QString::fromUtf8(load_key);
    r.stream.EndObject();
    preset.data = r.toByteArray();

    collector->target_presets->push_back(std::move(preset));
    return true;
  };

  metadata_receiver.add_plugin_id
      = [](const clap_preset_discovery_metadata_receiver_t* receiver,
           const clap_universal_plugin_id_t* plugin_id) { SCORE_TODO; };

  metadata_receiver.set_soundpack_id
      = [](const clap_preset_discovery_metadata_receiver_t* receiver,
           const char* soundpack_id) { SCORE_TODO; };

  metadata_receiver.set_flags
      = [](const clap_preset_discovery_metadata_receiver_t* receiver, uint32_t flags) {
    SCORE_TODO;
  };

  metadata_receiver.add_creator
      = [](const clap_preset_discovery_metadata_receiver_t* receiver,
           const char* creator) { SCORE_TODO; };

  metadata_receiver.set_description
      = [](const clap_preset_discovery_metadata_receiver_t* receiver,
           const char* description) { SCORE_TODO; };

  metadata_receiver.set_timestamps =
      [](const clap_preset_discovery_metadata_receiver_t* receiver,
         clap_timestamp creation_time, clap_timestamp modification_time) { SCORE_TODO; };

  metadata_receiver.add_feature
      = [](const clap_preset_discovery_metadata_receiver_t* receiver,
           const char* feature) { SCORE_TODO; };

  metadata_receiver.add_extra_info
      = [](const clap_preset_discovery_metadata_receiver_t* receiver, const char* key,
           const char* value) { SCORE_TODO; };

  // Enumerate all preset providers
  uint32_t provider_count = preset_discovery_factory->count(preset_discovery_factory);
  for(uint32_t i = 0; i < provider_count; ++i)
  {
    auto provider_desc = preset_discovery_factory->get_descriptor(preset_discovery_factory, i);
    if(!provider_desc)
      continue;

    auto provider = preset_discovery_factory->create(preset_discovery_factory, &indexer, provider_desc->id);
    if(!provider)
      continue;

    // Initialize the provider
    if(provider->init && provider->init(provider))
    {
      for(auto loc : collector.locations)
      {
        if(provider->get_metadata)
        {
          if(loc->location && strlen(loc->location))
          {
            provider->get_metadata(
                provider, CLAP_PRESET_DISCOVERY_LOCATION_FILE, loc->location,
                &metadata_receiver);
          }
          else
          {
            provider->get_metadata(
                provider, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr,
                &metadata_receiver);
          }
        }
      }
    }

    // Clean up the provider
    if(provider->destroy)
      provider->destroy(provider);
  }

  return presets;
}

void Model::flushFromPluginToHost()
{
  auto plugin = m_plugin->plugin;
  auto params
      = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
  if(!params)
    return;

  std::size_t control_idx = 0;
  for(auto* inlet : inlets())
  {
    if(auto* control = qobject_cast<Process::ControlInlet*>(inlet))
    {
      if(control_idx < m_plugin->m_parameters_ins.size())
      {
        const auto& param_info = m_plugin->m_parameters_ins[control_idx];
        double current_value = 0.0;

        // Read current parameter value from plugin
        // Note how here the plug-in changed its values: we want to save this in the data model.
        if(params->get_value(plugin, param_info.id, &current_value))
        {
          control->setValue(current_value);
        }
      }
      control_idx++;
    }
  }

  if(this->executing())
    return;
  /*
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
*/
  clap_input_events_t ip;
  ip.ctx = this;
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
      .ctx = this,
      .try_push = [](const struct clap_output_events* list,
                     const clap_event_header_t* event) { return false; }};
  params->flush(plugin, &ip, &op);
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
  QByteArray state;
  if(proc.m_plugin && proc.m_plugin->plugin)
  {
    state = Clap::readCLAPState(*proc.m_plugin->plugin);
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
    state = Clap::readCLAPState(*proc.m_plugin->plugin);
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
