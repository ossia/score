#pragma once
#include <Protocols/Bitfocus/BitfocusContext.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QObject>

#include <atomic>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>

namespace ossia::net
{
class bitfocus_protocol
    : public QObject
    , public ossia::net::protocol_base
{
public:
  bitfocus_protocol(
      std::shared_ptr<bitfocus::module_handler> rc,
      ossia::net::network_context_ptr ctx);
  ~bitfocus_protocol();

  bool pull(ossia::net::parameter_base&) override;
  bool push(const ossia::net::parameter_base& p, const ossia::value& v) override;
  bool push_raw(const ossia::net::full_parameter_data&) override;
  bool observe(ossia::net::parameter_base&, bool) override;
  bool update(ossia::net::node_base& node_base) override;
  void set_device(ossia::net::device_base& dev) override;

  //! Creates or updates the tree from the module's current definitions
  void init_device(int categories = bitfocus::module_handler::All);

  //! The value an option parameter starts with
  static ossia::value optionDefault(const bitfocus::module_data::config_field& opt);

  //! Option values of an action or feedback node, typed as the module expects
  QVariantMap collectOptions(
      const ossia::net::node_base& node,
      const std::vector<bitfocus::module_data::config_field>& defs,
      const std::string& group);

private:
  void run_action(const std::string& id);
  void forget_touched(const std::string& group, const std::string& name);
  void sync_actions();
  void sync_feedbacks();
  void sync_variables();
  void sync_variable(
      std::unordered_map<std::string, ossia::net::node_base*>& index, const QString& id,
      const bitfocus::module_data::variable_definition& def);
  void sync_options(
      ossia::net::node_base& node,
      const std::vector<bitfocus::module_data::config_field>& options);
  void subscribe_feedbacks(const std::vector<std::string>& ids);
  void set_received_value(ossia::net::parameter_base& p, ossia::value v);

  std::shared_ptr<bitfocus::module_handler> m_rc;
  ossia::net::network_context_ptr m_context;
  ossia::net::device_base* m_dev{};
  struct
  {
    ossia::net::node_base* actions{};
    ossia::net::node_base* feedbacks{};
    ossia::net::node_base* variables{};
  } nodes;

  // What push() may compare against from the execution thread
  std::atomic<ossia::net::node_base*> m_actionsNode{};
  std::atomic<ossia::net::node_base*> m_feedbacksNode{};

  // Options set through the tree: {"action" or "feedback", node, option}
  std::set<std::tuple<std::string, std::string, std::string>> m_touched;
  std::mutex m_touchedMutex;

  std::unordered_map<QString, ossia::net::parameter_base*> m_variables_recv;
  std::unordered_map<QString, ossia::net::parameter_base*> m_feedbacks_recv;
};
}
