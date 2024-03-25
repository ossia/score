#ifndef AVND_PROTOCOL_H
#define AVND_PROTOCOL_H

#include <QObject>

#include <verdigris>

#include <ossia/network/generic/wrapped_parameter.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/detail/logger.hpp>

namespace oscr
{
struct avnd_parameter_data final
    : public ossia::net::parameter_data
{
  avnd_parameter_data() = default;
  avnd_parameter_data(const avnd_parameter_data&) = delete;
  avnd_parameter_data(avnd_parameter_data&&) = default;
  avnd_parameter_data& operator=(const avnd_parameter_data&) = delete;
  avnd_parameter_data& operator=(avnd_parameter_data&&) = delete;

  avnd_parameter_data(const std::string& name)
    : parameter_data{name}
  {
  }

  bool valid() const noexcept
  {
    if (type) return true;
    return false;
  }
};

using avnd_parameter = ossia::net::wrapped_parameter<avnd_parameter_data>;
using avnd_node = ossia::net::wrapped_node<avnd_parameter_data, avnd_parameter>;

class avnd_protocol final
    : public QObject
    , public ossia::net::protocol_base
{
public:
  avnd_protocol() {};

  ~avnd_protocol() override;

  bool pull(ossia::net::parameter_base&) override { return false; };
  bool push(const ossia::net::parameter_base& parameter_base,
            const ossia::value& v) override { return false; };
  bool push_raw(const ossia::net::full_parameter_data& parameter_base) override { return false; };
  void set_device(ossia::net::device_base& dev) override { m_device = &dev; };
  bool observe(ossia::net::parameter_base&, bool b) override { return false; };
  bool update(ossia::net::node_base& node_base) override { return true; };

private:
  ossia::net::device_base* m_device{};
  QObject* m_object{};
};

}
#endif // AVND_PROTOCOL_H
