
#include "lv2_atom_helpers.hpp"

#if defined(__clang__) && defined(__has_warning)
#pragma clang diagnostic push
#if __has_warning("-Wdeprecated-declarations")
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif
#include <LV2/Context.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <lilv/lilv.h>
#include <lilv/lilvmm.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace LV2
{
namespace
{
struct WorkerData
{
  LV2::EffectContext& effect;

  uint32_t size;
  const void* data;
};

static LV2_URID do_uri_map(LV2_URI_Map_Callback_Data ptr, const char*, const char* val)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  auto& map = c.uri_map_left;

  std::string v{val};
  auto it = map.find(v);
  if(it != map.end())
  {
    return it->second;
  }
  else
  {
    auto n = c.uri_map_cur;
    map.insert(std::make_pair(v, n));
    c.uri_map_cur++;
    return n;
  }
}

static LV2_URID do_map(LV2_URID_Map_Handle ptr, const char* val)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  auto& map = c.urid_map_left;

  std::string v{val};
  auto it = map.find(v);
  if(it != map.end())
  {
    return it->second;
  }
  else
  {
    auto n = c.urid_map_cur;
    map.insert(std::make_pair(v, n));
    c.urid_map_right.insert(std::make_pair(n, v));
    c.urid_map_cur++;
    return n;
  }
}

static const char* do_unmap(LV2_URID_Unmap_Handle ptr, LV2_URID val)
{
  auto& c = *static_cast<LV2::GlobalContext*>(ptr);
  auto& map = c.urid_map_right;

  auto it = map.find(val);
  if(it != map.end())
  {
    return it->second.data();
  }
  else
  {
    return (const char*)nullptr;
  }
}

static LV2_Worker_Status
do_worker(LV2_Worker_Schedule_Handle, uint32_t, const void*)
{
  return LV2_WORKER_ERR_UNKNOWN;
}

static LV2_Worker_Status
do_worker_state(LV2_Worker_Schedule_Handle ptr, uint32_t s, const void* data)
{
  return LV2_WORKER_ERR_UNKNOWN;
}

static int lv2_printf(LV2_Log_Handle handle, LV2_URID type, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int r = std::vprintf(format, args);
  va_end(args);
  return r;
}

static uint32_t do_event_ref(LV2_Event_Callback_Data callback_data, LV2_Event* event)
{
  SCORE_TODO;
  return 0;
}

static uint32_t do_event_unref(LV2_Event_Callback_Data callback_data, LV2_Event* event)
{
  SCORE_TODO;
  return 0;
}

static LV2_Resize_Port_Status
do_port_resize(LV2_Resize_Port_Feature_Data data, uint32_t index, size_t size)
{
  SCORE_TODO;
  return LV2_RESIZE_PORT_ERR_UNKNOWN;
}

static char* do_make_path(LV2_State_Make_Path_Handle handle, const char* path)
{
  SCORE_TODO;
  return nullptr;
}
}

void InstanceWorker::start(WorkerThread& t, const LV2_Worker_Interface* i)
{
  requests = std::make_unique<queue>(queue_size);
  responses = std::make_unique<queue>(queue_size);
  request_scratch = std::make_unique<WorkerPacket>();
  response_scratch = std::make_unique<WorkerPacket>();
  iface = i;
  thread = &t;
}

namespace
{
LV2_Worker_Status
push_packet(InstanceWorker::queue& q, uint32_t size, const void* data) noexcept
{
  if(size > WorkerPacket::capacity)
    return LV2_WORKER_ERR_NO_SPACE;
  WorkerPacket p;
  p.size = size;
  if(size > 0)
    std::memcpy(p.data, data, size);
  return q.try_enqueue(std::move(p)) ? LV2_WORKER_SUCCESS : LV2_WORKER_ERR_NO_SPACE;
}
}

LV2_Worker_Status InstanceWorker::schedule_work(
    LV2_Worker_Schedule_Handle h, uint32_t size, const void* data) noexcept
{
  auto& self = *static_cast<InstanceWorker*>(h);
  if(!self.thread || !self.requests)
    return LV2_WORKER_ERR_UNKNOWN;
  const auto st = push_packet(*self.requests, size, data);
  if(st == LV2_WORKER_SUCCESS)
    self.thread->notify();
  return st;
}

LV2_Worker_Status InstanceWorker::respond(
    LV2_Worker_Respond_Handle h, uint32_t size, const void* data) noexcept
{
  auto& self = *static_cast<InstanceWorker*>(h);
  return push_packet(*self.responses, size, data);
}

void InstanceWorker::run_works(LV2_Handle handle) noexcept
{
  auto& p = *request_scratch;
  while(requests->try_dequeue(p))
    iface->work(handle, &InstanceWorker::respond, this, p.size, p.data);
}

void InstanceWorker::deliver_responses(LV2_Handle handle) noexcept
{
  if(!responses || !iface->work_response)
    return;
  auto& p = *response_scratch;
  while(responses->try_dequeue(p))
    iface->work_response(handle, p.size, p.data);
}

void InstanceDeleter::operator()(InstanceHandle* h) const noexcept
{
  if(auto app = qApp; app && QThread::currentThread() != app->thread())
    QMetaObject::invokeMethod(app, [h] { delete h; });
  else
    delete h;
}

WorkerThread::WorkerThread()
    : m_thread{[this] {
      ossia::set_thread_name("ossia lv2 work");
      while(m_running.load(std::memory_order_acquire))
      {
        m_sem.wait();
        run_pending();
      }
      // A request scheduled with success is passed to work()
      run_pending();
    }}
{
}

WorkerThread::~WorkerThread()
{
  m_running.store(false, std::memory_order_release);
  m_sem.signal();
  m_thread.join();
}

void WorkerThread::add(const SharedInstance& h)
{
  std::lock_guard l{m_lock};
  std::erase_if(m_instances, [](const auto& w) { return w.expired(); });
  m_instances.push_back(h);
}

void WorkerThread::run_pending()
{
  {
    std::lock_guard l{m_lock};
    std::erase_if(m_instances, [this](const auto& w) {
      auto h = w.lock();
      if(!h)
        return true;
      if(h->worker.requests->size_approx() > 0)
        m_pending.push_back(std::move(h));
      return false;
    });
  }

  // Not under the lock: the main thread adds instances meanwhile
  for(auto& h : m_pending)
  {
    try
    {
      h->worker.run_works(h->instance->lv2_handle);
    }
    catch(...)
    {
    }
  }
  // The last reference to an instance may be one of these: InstanceDeleter
  // frees it on the main thread
  m_pending.clear();
}

SharedInstance GlobalContext::instantiate(const LilvPlugin* plugin, double rate)
{
  SharedInstance h{new InstanceHandle, InstanceDeleter{}};
  for(auto* f : lv2_features)
  {
    if(!f)
      break;
    h->features.push_back(f == &worker_feature ? &h->worker.feature : f);
  }
  h->features.push_back(nullptr);

  h->instance = lilv_plugin_instantiate(plugin, rate, h->features.data());
  if(!h->instance)
    return {};

  if(lilv_plugin_has_extension_data(plugin, host.work_interface.me))
  {
    auto iface = static_cast<const LV2_Worker_Interface*>(
        lilv_instance_get_extension_data(h->instance, LV2_WORKER__interface));
    if(iface && iface->work)
    {
      h->worker.start(works, iface);
      works.add(h);
    }
  }
  return h;
}

GlobalContext::GlobalContext(int buffer_size, LV2::HostContext& host)
    : host{host}
    , uri_map{this, do_uri_map}
    , map{this, do_map}
    , unmap{this, do_unmap}
    , event{this, do_event_ref, do_event_unref}
    , worker{this, do_worker}
    , worker_state{this, do_worker_state}
    , logger{this,
             lv2_printf,
             [](auto h, auto t, auto fmt, auto lst) {
               return std::vprintf(fmt, lst);
             }}
    , ext_port_resize{this, do_port_resize}
    , state_make_path{this, do_make_path}
{
  options.reserve(8);

  static const int min = 0;
  static const int max = 4096;
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE, 0, map.map(map.handle, LV2_BUF_SIZE__minBlockLength),
      sizeof(min), map.map(map.handle, LV2_ATOM__Int), &min});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE, 0, map.map(map.handle, LV2_BUF_SIZE__maxBlockLength),
      sizeof(max), map.map(map.handle, LV2_ATOM__Int), &max});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE, 0, map.map(map.handle, LV2_CORE__sampleRate),
      sizeof(sampleRate), map.map(map.handle, LV2_ATOM__Double), &sampleRate});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE, 0, map.map(map.handle, LV2_PARAMETERS__sampleRate),
      sizeof(paramSampleRate), map.map(map.handle, LV2_ATOM__Float),
      &paramSampleRate});
  options.push_back(LV2_Options_Option{
      LV2_OPTIONS_INSTANCE, 0, map.map(map.handle, LV2_BUF_SIZE__sequenceSize),
      sizeof(host.midi_buffer_size), map.map(map.handle, LV2_ATOM__Int),
      &host.midi_buffer_size});

  options.push_back(LV2_Options_Option{LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr});

  options_feature.data = options.data();

  lv2_features.push_back(&uri_map_feature);
  lv2_features.push_back(&map_feature);
  lv2_features.push_back(&unmap_feature);
  lv2_features.push_back(&event_feature);
  lv2_features.push_back(&options_feature);
  lv2_features.push_back(&worker_feature);
  lv2_features.push_back(&logger_feature);
  // lv2_features.push_back(&ext_data_feature);
  // lv2_features.push_back(&ext_port_resize_feature);
  // lv2_features.push_back(&state_make_path_feature);
  // lv2_features.push_back(&state_load_default_feature);
  // lv2_features.push_back(&state_thread_safe_restore_feature);
  lv2_features.push_back(&bounded);
  lv2_features.push_back(&pow2);
  lv2_features.push_back(nullptr); // must be a null-terminated array per LV2 API.
}

void LV2::GlobalContext::loadPlugins()
{
#if defined(__linux__)
  if(!qEnvironmentVariableIsSet("LV2_PATH"))
  {
    QStringList lv2_paths;

    if(auto home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
       QFile::exists(home + "/.lv2"))
      lv2_paths.push_back(home + "/.lv2");

    if(QFile::exists(QStringLiteral("/usr/lib/lv2")))
      lv2_paths.push_back(QStringLiteral("/usr/lib/lv2"));
    if(QFile::exists(QStringLiteral("/usr/local/lib/lv2")))
      lv2_paths.push_back(QStringLiteral("/usr/local/lib/lv2"));
    if(QFile::exists(QStringLiteral("/usr/lib64/lv2")))
      lv2_paths.push_back(QStringLiteral("/usr/lib64/lv2"));
    if(QFile::exists(QStringLiteral("/usr/local/lib64/lv2")))
      lv2_paths.push_back(QStringLiteral("/usr/local/lib64/lv2"));

    QProcess proc;
    proc.setProgram("ldconfig");
    proc.setArguments({"-p"});
    proc.start();
    proc.waitForFinished();
    auto lst = proc.readAllStandardOutput().split('\n');

    for(auto& l : lst)
    {
      if(l.contains("libc.so.6"))
      {
#if defined(__LP64__)
        if(!l.contains("64"))
          continue;
#endif
        auto parts = l.split('>');
        if(parts.size() == 2)
        {
          QFileInfo libc{parts[1].trimmed()};
          if(!libc.isDir())
            libc = QFileInfo{libc.absoluteFilePath()};
          if(QFileInfo lv2_folder{libc.path() + "/lv2"};
             lv2_folder.exists() && lv2_folder.isDir())
          {
            lv2_paths.push_back(lv2_folder.absoluteFilePath());
          }
        }
      }
    }

    for(auto& path : lv2_paths)
      path = QFileInfo{path}.canonicalFilePath();
    lv2_paths.sort();
    lv2_paths.removeDuplicates();

    auto paths = lv2_paths.join(":");
    qDebug() << "LV2 paths: " << paths;

    qputenv("LV2_PATH", paths.toUtf8());
  }
#endif

  mapHostUrids();
}

void LV2::GlobalContext::mapHostUrids()
{
  host.null_id = map.map(map.handle, "");
  host.midi_event_id = map.map(map.handle, LILV_URI_MIDI_EVENT);
  host.atom_sequence_id = map.map(map.handle, LV2_ATOM__Sequence);
  host.atom_object_id = map.map(map.handle, LV2_ATOM__Object);
  host.atom_chunk_id = map.map(map.handle, LV2_ATOM__Chunk);
  host.atom_eventTransfer = map.map(map.handle, LV2_ATOM__eventTransfer);

  host.time_Time_id = map.map(map.handle, LV2_TIME__Time);
  host.time_Position_id = map.map(map.handle, LV2_TIME__Position);
  host.time_rate_id = map.map(map.handle, LV2_TIME__Rate);
  //host.time_position_id = map.map(map.handle, LV2_TIME__position);
  host.time_barBeat_id = map.map(map.handle, LV2_TIME__barBeat);
  host.time_bar_id = map.map(map.handle, LV2_TIME__bar);
  host.time_beat_id = map.map(map.handle, LV2_TIME__beat);
  host.time_beatUnit_id = map.map(map.handle, LV2_TIME__beatUnit);
  host.time_beatsPerBar_id = map.map(map.handle, LV2_TIME__beatsPerBar);
  host.time_beatsPerMinute_id = map.map(map.handle, LV2_TIME__beatsPerMinute);
  host.time_frame_id = map.map(map.handle, LV2_TIME__frame);
  host.time_framesPerSecond_id = map.map(map.handle, LV2_TIME__framesPerSecond);
  host.time_speed_id = map.map(map.handle, LV2_TIME__speed);

  // For state:port-restore decoding; see lv2_set_port_value in EffectModel.cpp
  host.atom_Float_id = map.map(map.handle, LV2_ATOM__Float);
  host.atom_Double_id = map.map(map.handle, LV2_ATOM__Double);
  host.atom_Int_id = map.map(map.handle, LV2_ATOM__Int);
  host.atom_Long_id = map.map(map.handle, LV2_ATOM__Long);
}

LV2Data::LV2Data(HostContext& h, EffectContext& ctx)
    : host{h}
    , effect{ctx}
{
  const auto numports = effect.plugin.get_num_ports();
  atom_minimum_sizes.assign(numports, 0);
  for(std::size_t i = 0; i < numports; i++)
  {
    Lilv::Port port = effect.plugin.get_port_by_index(i);

    // A plug-in which sends more than a few events at once (a whole pattern
    // to its UI...) says how big its atom buffers must be; smaller ones make
    // it drop what does not fit.
    if(LilvNodes* sizes = lilv_port_get_value(
           effect.plugin.me, port.me, host.resize_minimum_size.me))
    {
      LILV_FOREACH(nodes, it, sizes)
      {
        const LilvNode* n = lilv_nodes_get(sizes, it);
        if(lilv_node_is_int(n) && lilv_node_as_int(n) > 0)
          atom_minimum_sizes[i] = std::max(atom_minimum_sizes[i], uint32_t(lilv_node_as_int(n)));
      }
      lilv_nodes_free(sizes);
    }

    auto cl = port.get_classes();
    auto debug_port = [&] {
      LilvNode* name = port.get_name(); // owned by the caller
      qDebug() << "Port " << i << " : " << lilv_node_as_string(name);
      lilv_node_free(name);
      auto beg = lilv_nodes_begin(cl);
      while(!lilv_nodes_is_end(cl, beg))
      {
        auto node = lilv_nodes_get(cl, beg);
        qDebug() << " --> " << lilv_node_as_string(node);
        beg = lilv_nodes_next(cl, beg);
      }
    };
    if(port.is_a(host.audio_class))
    {
      if(port.is_a(host.input_class))
      {
        audio_in_ports.push_back(i);
      }
      else if(port.is_a(host.output_class))
      {
        audio_out_ports.push_back(i);
      }
      else
      {
        cv_ports.push_back(i);
        qDebug() << "LV2: Audio port not input or output" << i;
        debug_port();
      }
    }
    else if(port.is_a(host.atom_class))
    {
      if(port.supports_event(host.midi_event_class))
      {
        if(port.is_a(host.input_class))
        {
          midi_in_ports.push_back(i);
        }
        else if(port.is_a(host.output_class))
        {
          midi_out_ports.push_back(i);
        }
        else
        {
          midi_other_ports.push_back(i);
          atom_other_ports.push_back(i);
          qDebug() << "LV2: MIDI port not input or output" << i;
          debug_port();
        }
      }
      else
      {
        if(port.is_a(host.input_class))
        {
          atom_in_ports.push_back(i);
        }
        else if(port.is_a(host.output_class))
        {
          atom_out_ports.push_back(i);
        }
        else
        {
          atom_other_ports.push_back(i);
          qDebug() << "LV2: Atom port not input or output" << i;
          debug_port();
        }
      }

      // The transport goes to the plug-in: an output that may carry a
      // position (Sherlock forwards what it receives) is not one of these
      if(port.is_a(host.input_class) && port.supports_event(host.time_Position_class))
      {
        time_Position_ports.push_back(i);
      }
    }
    else if(port.is_a(host.cv_class))
    {
      if(port.is_a(host.output_class))
        cv_out_ports.push_back(i);
      else
        cv_ports.push_back(i);
    }
    else if(port.is_a(host.control_class))
    {
      if(port.is_a(host.input_class))
      {
        control_in_ports.push_back(i);
      }
      else if(port.is_a(host.output_class))
      {
        control_out_ports.push_back(i);
      }
      else
      {
        control_other_ports.push_back(i);
        qDebug() << "LV2: Control port not input or output" << i;
        debug_port();
      }
    }
    else
    {
      control_other_ports.push_back(i);
      qDebug() << "LV2: cannot categorize port" << i;
      debug_port();
    }
  }
}

bool LV2Data::isSampleRatePort(int port_index) const
{
  const LilvPort* port = lilv_plugin_get_port_by_index(effect.plugin.me, port_index);
  return port
         && lilv_port_has_property(effect.plugin.me, port, host.sampleRate_property.me);
}

void LV2Data::portRanges(
    ossia::float_vector& min, ossia::float_vector& max,
    ossia::float_vector& init) const
{
  const std::size_t n = effect.plugin.get_num_ports();
  min.resize(n);
  max.resize(n);
  init.resize(n);
  effect.plugin.get_port_ranges_float(min.data(), max.data(), init.data());

  // Blop's Retriggerable ADSR gives its times a minimum only
  for(std::size_t i = 0; i < n; i++)
  {
    float& lo = min[i];
    float& hi = max[i];
    float& def = init[i];
    const bool has_def = std::isfinite(def);
    if(!std::isfinite(lo))
      lo = std::isfinite(hi) ? std::min(0.f, hi - 1.f) : 0.f;
    if(!std::isfinite(hi) || hi < lo)
      hi = (has_def && def > lo) ? std::max(lo + 1.f, lo + 2.f * (def - lo))
                                 : lo + 1.f;
    def = has_def ? std::clamp(def, lo, hi) : lo;
  }
}
}

#if defined(__clang__) && defined(__has_warning)
#pragma clang diagnostic pop
#endif
