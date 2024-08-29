#pragma once
#include <score/tools/std/StringHash.hpp>

#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/network/common/path.hpp>

#include <QObject>

#include <verdigris>
namespace Device
{
class DeviceList;
}
namespace ossia
{
struct execution_state;
namespace net
{
class device_base;
}
}
namespace JS
{
using DeviceCache = ossia::small_vector<ossia::net::device_base*, 4>;
using DevicePushFunction
    = smallfun::function<void(ossia::net::parameter_base&, const ossia::value_port&)>;

class ExecStateWrapper : public QObject
{
  W_OBJECT(ExecStateWrapper)
public:
  ExecStateWrapper(const DeviceCache& state, DevicePushFunction push, QObject* parent)
      : QObject{parent}
      , devices{state}
      , on_push{std::move(push)}
  {
  }
  ~ExecStateWrapper() override;

  QVariant read(const QString& address);
  W_SLOT(read);
  void write(const QString& address, const QVariant& value);
  W_SLOT(write);
  void exec(const QString& code) W_SIGNAL(exec, code);
  void compute(const QString& code, const QString& cb) W_SIGNAL(compute, code, cb);
  void system(const QString& code) W_SIGNAL(system, code);

  /// Conversions ///
  QVariant asArray(QVariant) const noexcept;
  W_SLOT(asArray)
  QVariant asColor(QVariant) const noexcept;
  W_SLOT(asColor)
  QVariant asVec2(QVariant) const noexcept;
  W_SLOT(asVec2)
  QVariant asVec3(QVariant) const noexcept;
  W_SLOT(asVec3)
  QVariant asVec4(QVariant) const noexcept;
  W_SLOT(asVec4)

  static ossia::net::node_base* find_node(DeviceCache& devices, std::string_view name);
  const ossia::destination_t& find_address(const QString&);

  DeviceCache devices;

private:
  DevicePushFunction on_push;

  ossia::hash_map<QString, ossia::destination_t> m_address_cache;
  ossia::value_port m_port_cache;
  // TODO share cache
};

/*
class EditStateWrapper : public QObject
{
  W_OBJECT(EditStateWrapper)
public:
  EditStateWrapper(Device::DeviceList& state, QObject* parent)
      : QObject{parent}
      , devices{state}
  {
  }
  ~EditStateWrapper() override;

  QVariant read(const QString& address);
  W_SLOT(read);
  void write(const QString& address, const QVariant& value);
  W_SLOT(write);

private:
  Device::DeviceList& devices;

  const ossia::destination_t& find_address(const QString&);
  ossia::hash_map<QString, ossia::destination_t> m_address_cache;
  ossia::value_port m_port_cache;
  // TODO share cache
};
*/
}
