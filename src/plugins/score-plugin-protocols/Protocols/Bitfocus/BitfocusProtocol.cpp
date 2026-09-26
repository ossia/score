#include "BitfocusProtocol.hpp"

#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_parameter.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QPointer>

namespace ossia::net
{
namespace
{
using config_field = bitfocus::module_data::config_field;

ossia::val_type optionType(const config_field& opt)
{
  if(opt.type == "number")
    return opt.isInteger() ? ossia::val_type::INT : ossia::val_type::FLOAT;
  if(opt.type == "checkbox")
    return ossia::val_type::BOOL;
  if(opt.type == "multidropdown")
    return ossia::val_type::LIST;
  if(opt.type == "colorpicker" && !opt.default_json.isString()
     && opt.returnType != "string")
    return ossia::val_type::INT;
  return ossia::val_type::STRING;
}

void applyDomain(ossia::net::parameter_base& p, const config_field& opt)
{
  if(opt.type == "number")
  {
    if(opt.min.isValid() && opt.max.isValid())
    {
      if(p.get_value_type() == ossia::val_type::INT)
        p.set_domain(ossia::make_domain(opt.min.toInt(), opt.max.toInt()));
      else
        p.set_domain(ossia::make_domain(opt.min.toFloat(), opt.max.toFloat()));
    }
  }
  else if(opt.type == "dropdown" || opt.type == "multidropdown")
  {
    auto dom = ossia::domain_base<std::string>{};
    for(const auto& choice : opt.choices)
      dom.values.push_back(choice.id.toStdString());
    p.set_domain(std::move(dom));
  }
}

//! Numbers keep a single ossia type as long as they are integral and fit
ossia::value receivedValue(const QVariant& v)
{
  switch(v.typeId())
  {
    case QMetaType::Double: {
      const double d = v.toDouble();
      if(std::floor(d) == d && std::abs(d) < (1 << 30))
        return (int)d;
      return (float)d;
    }
    case QMetaType::LongLong:
    case QMetaType::ULongLong: {
      const auto i = v.toLongLong();
      if(std::abs(i) < (1 << 30))
        return (int)i;
      return (float)i;
    }
    default:
      return ossia::qt::qt_to_ossia{}(v);
  }
}

std::unordered_map<std::string, ossia::net::node_base*>
childIndex(ossia::net::node_base& n)
{
  std::unordered_map<std::string, ossia::net::node_base*> res;
  const auto& cld = n.children();
  res.reserve(cld.size());
  for(auto& c : cld)
    res.emplace(c->get_name(), c.get());
  return res;
}

ossia::net::node_base& findOrCreate(
    ossia::net::node_base& parent,
    std::unordered_map<std::string, ossia::net::node_base*>& index,
    const std::string& name)
{
  if(auto it = index.find(name); it != index.end())
    return *it->second;
  auto n = parent.create_child(name);
  index.emplace(name, n);
  return *n;
}

void setDescription(ossia::net::node_base& n, const QString& desc)
{
  auto str = desc.toStdString();
  if(ossia::net::get_description(n) != str)
    ossia::net::set_description(n, std::move(str));
}

//! Removes the children whose name is not a key of `model`
template <typename Map>
void removeGone(
    ossia::net::node_base& parent, const Map& model,
    const std::function<void(const std::string&)>& onRemove = {})
{
  std::vector<std::string> gone;
  for(auto& c : parent.children())
    if(!model.contains(QString::fromStdString(c->get_name())))
      gone.push_back(c->get_name());
  for(auto& name : gone)
  {
    if(onRemove)
      onRemove(name);
    parent.remove_child(name);
  }
}

ossia::val_type feedbackType(const bitfocus::module_data::feedback_definition& fb)
{
  if(fb.type == "boolean")
    return ossia::val_type::BOOL;
  if(fb.type == "advanced")
    return ossia::val_type::MAP;
  return ossia::val_type::STRING;
}
}

ossia::value bitfocus_protocol::optionDefault(const config_field& opt)
{
  const auto def = bitfocus::defaultOptionValue(opt);
  switch(optionType(opt))
  {
    case ossia::val_type::INT:
      return (int)def.toDouble();
    case ossia::val_type::FLOAT:
      return (float)def.toDouble();
    case ossia::val_type::BOOL:
      return def.toBool();
    case ossia::val_type::LIST: {
      std::vector<ossia::value> res;
      if(def.isArray())
        for(const auto& v : def.toArray())
          res.push_back(v.toVariant().toString().toStdString());
      else if(!def.isUndefined() && !def.isNull())
        res.push_back(def.toVariant().toString().toStdString());
      return res;
    }
    default:
      if(def.isUndefined() || def.isNull())
        return std::string{};
      return def.toVariant().toString().toStdString();
  }
}

bitfocus_protocol::bitfocus_protocol(
    std::shared_ptr<bitfocus::module_handler> rc, ossia::net::network_context_ptr ctx)
    : m_rc{rc}
    , m_context{ctx}
{
  QObject::connect(
      m_rc.get(), &bitfocus::module_handler::variableChanged, this,
      [this](const QString& name, const QVariant& v) {
    if(auto it = m_variables_recv.find(name); it != m_variables_recv.end())
    {
      set_received_value(*it->second, receivedValue(v));
    }
    else if(m_dev)
    {
      auto& m = m_rc->model();
      if(auto def = m.variables.find(name); def != m.variables.end())
      {
        if(!nodes.variables)
          nodes.variables = m_dev->get_root_node().create_child("variable");
        auto index = childIndex(*nodes.variables);
        sync_variable(index, name, def->second);
      }
    }
  });

  QObject::connect(
      m_rc.get(), &bitfocus::module_handler::feedbackValueChanged, this,
      [this](const QString& id, const QString&, const QVariant& v) {
    if(auto it = m_feedbacks_recv.find(id); it != m_feedbacks_recv.end())
      set_received_value(*it->second, receivedValue(v));
  });

  QObject::connect(
      m_rc.get(), &bitfocus::module_handler::definitionsChanged, this,
      [this] { init_device(m_rc->takeChangedDefinitions()); });

  // A restarted module has no feedback subscribed
  QObject::connect(
      m_rc.get(), &bitfocus::module_handler::reregistered, this,
      [this] { init_device(); });
}

bitfocus_protocol::~bitfocus_protocol() = default;

void bitfocus_protocol::set_received_value(ossia::net::parameter_base& p, ossia::value v)
{
  if(!v.valid())
    return;
  if(p.get_value_type() != v.get_type())
    p.set_value_type(v.get_type());
  p.set_value(std::move(v));
}

bool bitfocus_protocol::pull(ossia::net::parameter_base&)
{
  return true;
}

QVariantMap bitfocus_protocol::collectOptions(
    const ossia::net::node_base& node, const std::vector<config_field>& defs)
{
  QVariantMap options;
  for(const auto& opt : defs)
  {
    if(opt.type == "static-text")
      continue;
    auto cld = const_cast<ossia::net::node_base&>(node).find_child(opt.id.toStdString());
    if(!cld)
      continue;
    auto p = cld->get_parameter();
    if(!p)
      continue;
    const auto val = p->value();
    const QVariant qv = val.apply(ossia::qt::ossia_to_qvariant{});
    // Untouched: companion sends the default as declared, or nothing without one
    if(val == optionDefault(opt))
    {
      if(opt.hasDefault())
        options[opt.id] = bitfocus::defaultOptionValue(opt).toVariant();
      continue;
    }
    const auto jv = bitfocus::toModuleValue(opt, qv);
    if(!jv.isUndefined())
      options[opt.id] = jv.toVariant();
  }
  return options;
}

bool bitfocus_protocol::push(const ossia::net::parameter_base& p, const ossia::value& v)
{
  // Called from the execution thread: the definitions and the tree are only
  // read on the handler's thread
  auto& node = p.get_node();
  auto parent = node.get_parent();
  if(!parent)
    return false;

  auto actions = m_actionsNode.load();
  auto feedbacks = m_feedbacksNode.load();
  if(actions && parent == actions)
  {
    QMetaObject::invokeMethod(this, [this, id = node.get_name()] { run_action(id); });
  }
  else if(actions && parent->get_parent() == actions && parent->children_count() == 1)
  {
    QMetaObject::invokeMethod(this, [this, id = parent->get_name()] { run_action(id); });
  }
  else if(feedbacks && parent->get_parent() == feedbacks)
  {
    QMetaObject::invokeMethod(this, [this, id = parent->get_name()] {
      subscribe_feedbacks({id});
    });
  }
  return true;
}

void bitfocus_protocol::run_action(const std::string& id)
{
  if(!nodes.actions)
    return;
  auto node = nodes.actions->find_child(id);
  const auto& defs = m_rc->model().actions;
  auto it = defs.find(QString::fromStdString(id));
  if(!node || it == defs.end())
    return;
  m_rc->actionRun(id, collectOptions(*node, it->second.options));
}

bool bitfocus_protocol::push_raw(const ossia::net::full_parameter_data&)
{
  return true;
}
bool bitfocus_protocol::observe(ossia::net::parameter_base&, bool)
{
  return true;
}
bool bitfocus_protocol::update(ossia::net::node_base& node_base)
{
  return true;
}

void bitfocus_protocol::sync_options(
    ossia::net::node_base& node, const std::vector<config_field>& options)
{
  std::vector<std::string> expected;
  auto index = childIndex(node);
  for(const auto& opt : options)
  {
    if(opt.type == "static-text")
      continue;
    const auto name = opt.id.toStdString();
    expected.push_back(name);

    auto cld = &findOrCreate(node, index, name);
    setDescription(*cld, opt.label);

    const auto type = optionType(opt);
    auto p = cld->get_parameter();
    if(p && p->get_value_type() != type)
    {
      cld->remove_parameter();
      p = nullptr;
    }
    if(!p)
    {
      p = cld->create_parameter(type);
      p->set_value(optionDefault(opt));
    }
    applyDomain(*p, opt);
  }

  std::vector<std::string> extra;
  for(auto& c : node.children())
    if(std::find(expected.begin(), expected.end(), c->get_name()) == expected.end())
      extra.push_back(c->get_name());
  for(auto& name : extra)
    node.remove_child(name);
}

void bitfocus_protocol::sync_actions()
{
  auto& m = m_rc->model();
  auto& root = m_dev->get_root_node();
  if(m.actions.empty())
  {
    if(nodes.actions)
      root.remove_child("action");
    nodes.actions = nullptr;
    m_actionsNode = nullptr;
    return;
  }
  if(!nodes.actions)
  {
    nodes.actions = root.create_child("action");
    m_actionsNode = nodes.actions;
  }

  removeGone(*nodes.actions, m.actions);

  auto index = childIndex(*nodes.actions);
  for(auto& [id, def] : m.actions)
  {
    auto node = &findOrCreate(*nodes.actions, index, id.toStdString());
    setDescription(*node, def.name);
    if(!node->get_parameter())
      node->create_parameter(ossia::val_type::IMPULSE);
    sync_options(*node, def.options);
  }
}

void bitfocus_protocol::sync_feedbacks()
{
  auto& m = m_rc->model();
  auto& root = m_dev->get_root_node();

  std::map<QString, bitfocus::module_data::feedback_instance> changes;
  if(nodes.feedbacks)
    removeGone(*nodes.feedbacks, m.feedbacks, [&](const std::string& name) {
      const auto id = QString::fromStdString(name);
      m_feedbacks_recv.erase(id);
      bitfocus::module_data::feedback_instance inst;
      inst.id = id;
      inst.disabled = true;
      changes[id] = inst;
    });

  if(m.feedbacks.empty())
  {
    if(nodes.feedbacks)
      root.remove_child("feedback");
    nodes.feedbacks = nullptr;
    m_feedbacksNode = nullptr;
  }
  else
  {
    if(!nodes.feedbacks)
    {
      nodes.feedbacks = root.create_child("feedback");
      m_feedbacksNode = nodes.feedbacks;
    }

    std::vector<std::string> ids;
    auto index = childIndex(*nodes.feedbacks);
    for(auto& [id, def] : m.feedbacks)
    {
      const auto name = id.toStdString();
      auto node = &findOrCreate(*nodes.feedbacks, index, name);
      setDescription(*node, def.name);

      const auto type = feedbackType(def);
      auto p = node->get_parameter();
      if(p && def.type != "value" && p->get_value_type() != type)
      {
        node->remove_parameter();
        p = nullptr;
      }
      if(!p)
        p = node->create_parameter(type);
      m_feedbacks_recv[id] = p;

      sync_options(*node, def.options);
      ids.push_back(name);
    }
    subscribe_feedbacks(ids);
  }

  if(!changes.empty())
    m_rc->updateFeedbacks(changes);
}

void bitfocus_protocol::subscribe_feedbacks(const std::vector<std::string>& ids)
{
  if(!nodes.feedbacks)
    return;
  auto& m = m_rc->model();
  std::map<QString, bitfocus::module_data::feedback_instance> instances;
  for(auto& name : ids)
  {
    const auto id = QString::fromStdString(name);
    auto def = m.feedbacks.find(id);
    auto node = nodes.feedbacks->find_child(name);
    if(def == m.feedbacks.end() || !node)
      continue;

    bitfocus::module_data::feedback_instance inst;
    inst.id = id;
    inst.controlId = "feedback/" + id;
    inst.definitionId = id;
    inst.options = collectOptions(*node, def->second.options);
    instances[id] = std::move(inst);
  }
  if(!instances.empty())
    m_rc->updateFeedbacks(instances);
}

void bitfocus_protocol::sync_variables()
{
  auto& m = m_rc->model();
  auto& root = m_dev->get_root_node();
  if(m.variables.empty())
  {
    if(nodes.variables)
      root.remove_child("variable");
    nodes.variables = nullptr;
    m_variables_recv.clear();
    return;
  }
  if(!nodes.variables)
    nodes.variables = root.create_child("variable");

  removeGone(*nodes.variables, m.variables, [this](const std::string& name) {
    m_variables_recv.erase(QString::fromStdString(name));
  });

  auto index = childIndex(*nodes.variables);
  for(auto& [id, def] : m.variables)
    sync_variable(index, id, def);
}

void bitfocus_protocol::sync_variable(
    std::unordered_map<std::string, ossia::net::node_base*>& index, const QString& id,
    const bitfocus::module_data::variable_definition& def)
{
  auto node = &findOrCreate(*nodes.variables, index, id.toStdString());
  if(!def.name.isEmpty())
    setDescription(*node, def.name);

  auto p = node->get_parameter();
  if(!p)
  {
    auto val = receivedValue(def.value);
    p = node->create_parameter(
        val.valid() && val.get_type() != ossia::val_type::IMPULSE
            ? val.get_type()
            : ossia::val_type::STRING);
    if(val.valid())
      p->set_value(val);
  }
  m_variables_recv[id] = p;
}

void bitfocus_protocol::init_device(int categories)
{
  if(!m_dev)
    return;

  if(categories & bitfocus::module_handler::Actions)
    sync_actions();
  if(categories & bitfocus::module_handler::Feedbacks)
    sync_feedbacks();
  if(categories & bitfocus::module_handler::Variables)
    sync_variables();
}

void bitfocus_protocol::set_device(ossia::net::device_base& dev)
{
  m_dev = &dev;
}
}
