#if defined(OSSIA_PROTOCOL_GPS)
#include "GPSDevice.hpp"

#include "GPSSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/dylib_loader.hpp>
#include <ossia/detail/timer.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QDebug>

#include <gps.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::GPSDevice)

namespace ossia::net
{
class libgpsd
{
public:
  decltype(&::gps_open) open{};
  decltype(&::gps_close) close{};
  decltype(&::gps_send) send{};
  decltype(&::gps_read) read{};
  decltype(&::gps_hexdump) hexdump{};
  decltype(&::gps_hexpack) hexpack{};
  decltype(&::gps_unpack) unpack{};
  decltype(&::gps_waiting) waiting{};
  decltype(&::gps_stream) stream{};
  decltype(&::gps_mainloop) mainloop{};
  decltype(&::gps_data) data{};
  decltype(&::gps_errstr) errstr{};

  static const libgpsd& instance()
  {
    static const libgpsd self;
    return self;
  }

private:
  dylib_loader library;

  libgpsd()
      : library("libgps.so.30")
  {
    open = library.symbol<decltype(&::gps_open)>("gps_open");
    close = library.symbol<decltype(&::gps_close)>("gps_close");
    send = library.symbol<decltype(&::gps_send)>("gps_send");
    read = library.symbol<decltype(&::gps_read)>("gps_read");
    hexdump = library.symbol<decltype(&::gps_hexdump)>("gps_hexdump");
    hexpack = library.symbol<decltype(&::gps_hexpack)>("gps_hexpack");
    unpack = library.symbol<decltype(&::gps_unpack)>("gps_unpack");
    waiting = library.symbol<decltype(&::gps_waiting)>("gps_waiting");
    stream = library.symbol<decltype(&::gps_stream)>("gps_stream");
    mainloop = library.symbol<decltype(&::gps_mainloop)>("gps_mainloop");
    data = library.symbol<decltype(&::gps_data)>("gps_data");
    errstr = library.symbol<decltype(&::gps_errstr)>("gps_errstr");

    assert(open);
    assert(close);
    assert(send);
    assert(read);
    assert(hexdump);
    assert(hexpack);
    assert(unpack);
    assert(waiting);
    assert(stream);
    assert(mainloop);
    assert(data);
    assert(errstr);
  }
};

struct gps_protocol : public ossia::net::protocol_base
{
public:
  gps_protocol(
      const libgpsd& instance, const Protocols::GPSSpecificSettings& set,
      ossia::net::network_context_ptr ctx)
      : protocol_base{flags{}}
      , gps{instance}
      , m_context{std::move(ctx)}
      , m_timer{m_context->context}
  {
    using namespace std::literals;
    const int ret = gps.open(
        set.host.toStdString().c_str(), std::to_string(set.port).c_str(), &data);
    if(ret != 0)
      throw std::runtime_error("Could not open gpsd client: "s + gps.errstr(ret));

    constexpr float frequency = 50;
    m_timer.set_delay(std::chrono::milliseconds{
        static_cast<int>(1000.0f / static_cast<float>(frequency))});
  }

  ~gps_protocol()
  {
    // When you are done...
    gps.stream(&data, WATCH_DISABLE, nullptr);
    gps.close(&data);
    m_timer.stop();
  }

  void set_device(ossia::net::device_base& dev) override
  {
    m_device = &dev;

    auto& root = m_device->get_root_node();

    params.fix = ossia::create_parameter(root, "/gps/fix", "bool");
    params.fix_mode = ossia::create_parameter(root, "/gps/fix/mode", "string");
    params.lat = ossia::create_parameter(root, "/gps/lat", "float");
    params.lon = ossia::create_parameter(root, "/gps/lon", "float");
    params.satellites = ossia::create_parameter(root, "/gps/sat", "float");
    params.time = ossia::create_parameter(root, "/gps/time", "time");
    params.time_year = ossia::create_parameter(root, "/gps/time/year", "int");
    params.time_month = ossia::create_parameter(root, "/gps/time/month", "int");
    params.time_day = ossia::create_parameter(root, "/gps/time/day", "int");
    params.time_hour = ossia::create_parameter(root, "/gps/time/hour", "int");
    params.time_minute = ossia::create_parameter(root, "/gps/time/minute", "int");
    params.time_second = ossia::create_parameter(root, "/gps/time/second", "float");

    (void)gps.stream(&data, WATCH_ENABLE | WATCH_JSON, nullptr);
    m_timer.start([this] { update_function(); });
  }

  bool pull(parameter_base& v) override { return false; }

  bool push(const parameter_base& p, const value& v) override { return false; }
  bool push_raw(const full_parameter_data&) override { return false; }
  bool observe(parameter_base&, bool) override { return false; }
  bool update(node_base& node_base) override { return false; }

  void update_function()
  {
    static constexpr const char* mode_str[4] = {"n/a", "None", "2D", "3D"};

    while(gps.waiting(&data, 1))
    {
      if(-1 == gps.read(&data, nullptr, 0))
      {
        // FIXME stop the timer?
        return;
      }
      if(MODE_SET != (MODE_SET & data.set))
      {
        // did not even get mode, nothing to see here
        return;
      }
      if(0 > data.fix.mode || 4 <= data.fix.mode)
      {
        data.fix.mode = 0;
      }
      params.fix->push_value(data.fix.mode > 1);
      params.fix_mode->push_value(std::string(mode_str[data.fix.mode]));

      params.satellites->push_value(data.satellites_visible);
      if(TIME_SET == (TIME_SET & data.set))
      {
        const auto time = data.fix.time.tv_sec + 1e-9 * data.fix.time.tv_nsec;
        params.time->push_value(time);
        const auto tm = localtime(&data.fix.time.tv_sec);
        params.time_year->push_value(tm->tm_year + 1900);
        params.time_month->push_value(tm->tm_mon + 1);
        params.time_day->push_value(tm->tm_mday);
        params.time_hour->push_value(tm->tm_hour);
        params.time_minute->push_value(tm->tm_min);
        params.time_second->push_value(tm->tm_sec + 1e-9 * data.fix.time.tv_nsec);
      }

      if(std::isfinite(data.fix.latitude) && std::isfinite(data.fix.longitude))
      {
        params.lat->push_value(data.fix.latitude);
        params.lon->push_value(data.fix.longitude);
      }
    }
  }

  const libgpsd& gps;
  ossia::net::network_context_ptr m_context;
  ossia::net::device_base* m_device{};
  gps_data_t data;
  ossia::timer m_timer;

  struct
  {
    ossia::net::parameter_base* fix{};
    ossia::net::parameter_base* fix_mode{};
    ossia::net::parameter_base* lat{};
    ossia::net::parameter_base* lon{};
    ossia::net::parameter_base* satellites{};
    ossia::net::parameter_base* time{};
    ossia::net::parameter_base* time_year{};
    ossia::net::parameter_base* time_month{};
    ossia::net::parameter_base* time_day{};
    ossia::net::parameter_base* time_hour{};
    ossia::net::parameter_base* time_minute{};
    ossia::net::parameter_base* time_second{};
  } params;
};
}
namespace Protocols
{

GPSDevice::GPSDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

GPSDevice::~GPSDevice() { }

bool GPSDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set
        = m_settings.deviceSpecificSettings.value<GPSSpecificSettings>();
    const auto& gps = ossia::net::libgpsd::instance();
    {
      auto pproto = std::make_unique<ossia::net::gps_protocol>(gps, set, m_ctx);
      auto& proto = *pproto;
      auto dev = std::make_unique<ossia::net::generic_device>(
          std::move(pproto), settings().name.toStdString());
      m_dev = std::move(dev);
    }
    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::runtime_error& e)
  {
    qDebug() << "GPS error: " << e.what();
  }
  catch(...)
  {
    qDebug() << "GPS error";
  }

  return connected();
}

void GPSDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
#endif
