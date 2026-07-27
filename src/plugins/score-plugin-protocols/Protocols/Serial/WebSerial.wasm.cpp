#include <Protocols/Serial/WebSerial.hpp>

#if defined(OSSIA_PROTOCOL_SERIAL) && defined(__EMSCRIPTEN__)
#include <QDebug>

#include <emscripten/emscripten.h>
#include <emscripten/threading.h>

#include <mutex>

// clang-format off
EM_JS(void, score_serial_install, (), {
  if(Module.scoreSerial)
    return;

  const S = {
    ports: [],
    generation: 0,
    scanning: false,
    pending: false,
    armed: false,
    handles: new Map(),
    next: 1,
  };
  Module.scoreSerial = S;

  S.supported = function() {
    return typeof navigator !== 'undefined' && !!navigator.serial;
  };

  S.identify = function(list) {
    const counts = {};
    return list.map(function(p) {
      let info = {};
      try { info = p.getInfo() || {}; } catch(e) { info = {}; }
      const vid = (typeof info.usbVendorId === 'number') ? info.usbVendorId : -1;
      const pid = (typeof info.usbProductId === 'number') ? info.usbProductId : -1;
      const key = vid + ':' + pid;
      const n = (counts[key] || 0);
      counts[key] = n + 1;
      return {port: p, vid: vid, pid: pid, id: 'webserial:' + key + ':' + n};
    });
  };

  S.scan = function() {
    if(!S.supported())
    {
      S.generation++;
      return;
    }
    if(S.scanning)
      return;
    S.scanning = true;
    navigator.serial.getPorts().then(function(list) {
      S.ports = S.identify(list);
      S.scanning = false;
      S.generation++;
    }).catch(function() {
      S.scanning = false;
      S.generation++;
    });
  };

  S.activation = function() {
    if(typeof navigator === 'undefined' || !navigator.userActivation)
      return -1;
    return navigator.userActivation.isActive ? 1 : 0;
  };

  S.doRequest = function() {
    navigator.serial.requestPort({}).then(function() {
      S.pending = false;
      S.scan();
    }).catch(function(e) {
      S.pending = false;
      S.generation++;
    });
  };

  // requestPort() needs transient user activation. When the call already runs
  // inside one it goes out immediately; otherwise it is armed and the next
  // pointer event fires it from the DOM listener, which is activated by
  // construction.
  S.request = function() {
    if(!S.supported() || S.pending)
      return;
    S.pending = true;
    if(S.activation() !== 0)
    {
      S.doRequest();
      return;
    }
    if(S.armed)
      return;
    S.armed = true;
    const fire = function() {
      S.armed = false;
      window.removeEventListener('pointerdown', fire, true);
      window.removeEventListener('keydown', fire, true);
      if(S.pending)
        S.doRequest();
    };
    window.addEventListener('pointerdown', fire, true);
    window.addEventListener('keydown', fire, true);
  };

  if(typeof navigator !== 'undefined' && navigator.serial
     && navigator.serial.addEventListener)
  {
    navigator.serial.addEventListener('connect', function() { S.scan(); });
    navigator.serial.addEventListener('disconnect', function() { S.scan(); });
  }
});

EM_JS(int, score_serial_supported, (), {
  return Module.scoreSerial.supported() ? 1 : 0;
});

EM_JS(int, score_serial_activation, (), {
  return Module.scoreSerial.activation();
});

EM_JS(void, score_serial_scan, (), {
  Module.scoreSerial.scan();
});

EM_JS(int, score_serial_generation, (), {
  return Module.scoreSerial.generation;
});

EM_JS(int, score_serial_count, (), {
  return Module.scoreSerial.ports.length;
});

EM_JS(void, score_serial_port_id, (int i, char* buf, int cap), {
  const p = Module.scoreSerial.ports[i];
  stringToUTF8(p ? p.id : "", buf, cap);
});

EM_JS(int, score_serial_port_vid, (int i), {
  const p = Module.scoreSerial.ports[i];
  return p ? p.vid : -1;
});

EM_JS(int, score_serial_port_pid, (int i), {
  const p = Module.scoreSerial.ports[i];
  return p ? p.pid : -1;
});

EM_JS(void, score_serial_request, (), {
  Module.scoreSerial.request();
});

EM_JS(int, score_serial_request_pending, (), {
  return Module.scoreSerial.pending ? 1 : 0;
});

EM_JS(int, score_serial_open, (const char* portId, int baudRate), {
  const S = Module.scoreSerial;
  if(!S.supported())
    return 0;

  const id = UTF8ToString(portId);
  const entry = S.ports.find(function(p) { return p.id === id; });
  if(!entry)
    return 0;

  const handle = S.next++;
  const st = {
    status: 0, error: "", rx: [], rxlen: 0,
    port: entry.port, reader: null, writer: null, closed: false,
    wchain: Promise.resolve(),
  };
  S.handles.set(handle, st);

  const fail = function(e) {
    st.error = (e && e.name ? e.name : 'Error') + ': '
             + (e && e.message ? e.message : String(e));
    st.status = -1;
  };

  entry.port.open({baudRate: baudRate}).then(function() {
    if(st.closed)
    {
      entry.port.close().catch(function() {});
      return;
    }
    st.status = 1;
    if(entry.port.writable)
      st.writer = entry.port.writable.getWriter();
    if(!entry.port.readable)
      return;
    st.reader = entry.port.readable.getReader();
    const pump = function() {
      if(st.closed || !st.reader)
        return;
      st.reader.read().then(function(res) {
        if(res.value && res.value.length > 0)
        {
          st.rx.push(res.value);
          st.rxlen += res.value.length;
        }
        if(!res.done)
          pump();
      }).catch(function(e) {
        if(!st.closed)
          fail(e);
      });
    };
    pump();
  }).catch(fail);

  return handle;
});

EM_JS(int, score_serial_status, (int handle), {
  const st = Module.scoreSerial.handles.get(handle);
  return st ? st.status : -1;
});

EM_JS(void, score_serial_error, (int handle, char* buf, int cap), {
  const st = Module.scoreSerial.handles.get(handle);
  stringToUTF8(st ? st.error : "", buf, cap);
});

EM_JS(int, score_serial_read, (int handle, char* buf, int cap), {
  const st = Module.scoreSerial.handles.get(handle);
  if(!st || st.rxlen === 0 || cap <= 0)
    return 0;

  let written = 0;
  while(written < cap && st.rx.length > 0)
  {
    const chunk = st.rx[0];
    const take = Math.min(chunk.length, cap - written);
    HEAPU8.set(chunk.subarray(0, take), buf + written);
    written += take;
    if(take === chunk.length)
      st.rx.shift();
    else
      st.rx[0] = chunk.subarray(take);
  }
  st.rxlen -= written;
  return written;
});

EM_JS(void, score_serial_write, (int handle, const char* data, int size), {
  const st = Module.scoreSerial.handles.get(handle);
  if(!st || !st.writer || st.closed || size <= 0)
    return;

  const bytes = HEAPU8.slice(data, data + size);
  st.wchain = st.wchain.then(function() {
    if(st.closed || !st.writer)
      return;
    return st.writer.write(bytes);
  }).catch(function(e) {
    st.error = 'write: ' + (e && e.message ? e.message : String(e));
  });
});

EM_JS(void, score_serial_close, (int handle), {
  const S = Module.scoreSerial;
  const st = S.handles.get(handle);
  if(!st)
    return;
  S.handles.delete(handle);
  st.closed = true;

  const shut = function() {
    return st.port.close().catch(function() {});
  };

  const reader = st.reader;
  const writer = st.writer;
  st.reader = null;
  st.writer = null;

  let chain = st.wchain.catch(function() {});
  if(writer)
    chain = chain.then(function() { return writer.close().catch(function() {}); });
  if(reader)
    chain = chain.then(function() { return reader.cancel().catch(function() {}); });
  chain.then(shut);
});
// clang-format on

namespace Protocols::WebSerial
{
namespace
{
struct Cache
{
  std::mutex mutex;
  std::vector<PortInfo> ports;
};

Cache& cache() noexcept
{
  static Cache c;
  return c;
}

bool onMainThread() noexcept
{
  return emscripten_is_main_browser_thread();
}

void install() noexcept
{
  static bool done = false;
  if(!done)
  {
    score_serial_install();
    done = true;
  }
}

std::optional<uint16_t> toId(int v) noexcept
{
  if(v < 0)
    return std::nullopt;
  return static_cast<uint16_t>(v);
}
}

bool available() noexcept
{
  if(!onMainThread())
    return !cachedPorts().empty();
  install();
  return score_serial_supported() != 0;
}

Activation userActivation() noexcept
{
  if(!onMainThread())
    return Activation::Unsupported;
  install();
  return static_cast<Activation>(score_serial_activation());
}

void scan() noexcept
{
  if(!onMainThread())
    return;
  install();
  score_serial_scan();
}

int generation() noexcept
{
  if(!onMainThread())
    return 0;
  install();
  return score_serial_generation();
}

void refresh() noexcept
{
  if(!onMainThread())
    return;
  install();

  std::vector<PortInfo> found;
  const int n = score_serial_count();
  found.reserve(n);
  for(int i = 0; i < n; i++)
  {
    char id[128] = {};
    score_serial_port_id(i, id, sizeof(id));
    found.push_back(
        {id, toId(score_serial_port_vid(i)), toId(score_serial_port_pid(i))});
  }

  auto& c = cache();
  std::lock_guard lock{c.mutex};
  c.ports = std::move(found);
}

std::vector<PortInfo> cachedPorts() noexcept
{
  auto& c = cache();
  std::lock_guard lock{c.mutex};
  return c.ports;
}

void requestPort() noexcept
{
  if(!onMainThread())
    return;
  install();
  score_serial_request();
}

bool requestPending() noexcept
{
  if(!onMainThread())
    return false;
  install();
  return score_serial_request_pending() != 0;
}

int open(const std::string& id, int baudRate) noexcept
{
  if(!onMainThread())
    return 0;
  install();
  return score_serial_open(id.c_str(), baudRate);
}

int status(int handle) noexcept
{
  if(handle <= 0 || !onMainThread())
    return -1;
  return score_serial_status(handle);
}

std::string error(int handle) noexcept
{
  if(handle <= 0 || !onMainThread())
    return {};
  char buf[256] = {};
  score_serial_error(handle, buf, sizeof(buf));
  return buf;
}

int read(int handle, char* buf, int capacity) noexcept
{
  if(handle <= 0 || !onMainThread())
    return 0;
  return score_serial_read(handle, buf, capacity);
}

void write(int handle, const char* data, int size) noexcept
{
  if(handle <= 0 || !onMainThread())
    return;
  score_serial_write(handle, data, size);
}

void close(int handle) noexcept
{
  if(handle <= 0 || !onMainThread())
    return;
  score_serial_close(handle);
}
}
#endif
