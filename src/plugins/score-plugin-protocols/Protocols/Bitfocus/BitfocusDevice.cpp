// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/Bitfocus/BitfocusContext.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <memory>
namespace ossia::net
{
class bitfocus_protocol
    : public QObject
    , public ossia::net::protocol_base
{
public:
  bitfocus_protocol(
      std::shared_ptr<bitfocus::module_handler> rc, ossia::net::network_context_ptr ctx)
      : m_rc{rc}
      , m_context{ctx}
  {
    QObject::connect(
        m_rc.get(), &bitfocus::module_handler::variableChanged, this,
        [self = QPointer{this},
         ctx = m_context](const QString& name, const QVariant& v) {
      boost::asio::post(ctx->context, [self, name, val = ossia::qt::qt_to_ossia{}(v)] {
        if(self)
        {
          auto it = self->m_variables_recv.find(name);
          if(it != self->m_variables_recv.end())
          {
            it->second->set_value(std::move(val));
          }
        }
      });
    },
        Qt::DirectConnection);

    QObject::connect(
        m_rc.get(), &bitfocus::module_handler::feedbackValueChanged, this,
        [self = QPointer{this}, ctx = m_context](
            const QString& id, const QString& controlId, const QVariant& v) {
      boost::asio::post(ctx->context, [self, id, val = ossia::qt::qt_to_ossia{}(v)] {
        if(self)
        {
          auto it = self->m_feedbacks_recv.find(id);
          if(it != self->m_feedbacks_recv.end())
          {
            it->second->set_value(std::move(val));
          }
        }
      });
    },
        Qt::DirectConnection);
  }

  bool pull(ossia::net::parameter_base&) override { return true; }
  bool push(const ossia::net::parameter_base& p, const ossia::value& v) override
  {
    auto parent = p.get_node().get_parent();
    if(parent == nodes.actions)
    {
      ossia::value_map_type options;
      {
        const auto& cld = p.get_node().children();
        options.reserve(cld.size());
        for(auto& cld : cld)
          if(auto option_p = cld->get_parameter())
            options.emplace_back(cld->get_name(), option_p->value());
      }
      QMetaObject::invokeMethod(
          m_rc.get(),
          [m = m_rc, name = p.get_node().get_name(), options = std::move(options)] {
        QVariantMap options_map;
        for(auto& [k, v] : options)
          options_map[QString::fromStdString(k)]
              = v.apply(ossia::qt::ossia_to_qvariant{});

        m->actionRun(name, options_map);
      });
    }
    else if(
        parent && parent->get_parent() == nodes.actions && parent->children_count() == 1)
    {
      ossia::value_map_type options;
      options.reserve(1);
      options.emplace_back(p.get_node().get_name(), v);

      QMetaObject::invokeMethod(
          m_rc.get(),
          [m = m_rc, name = parent->get_name(), options = std::move(options)] {
        QVariantMap options_map;
        for(auto& [k, v] : options)
          options_map[QString::fromStdString(k)]
              = v.apply(ossia::qt::ossia_to_qvariant{});

        m->actionRun(name, options_map);
      });
    }
    // else if(parent == nodes.presets)
    // {
    // }
    else if(parent == nodes.feedbacks)
    {
    }
    return true;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return true; }
  bool observe(ossia::net::parameter_base&, bool) override { return true; }
  bool update(ossia::net::node_base& node_base) override { return true; }

  void setup_node(auto& config, ossia::net::node_base* node)
  {
    ossia::net::set_description(*node, config.name.toStdString());
    switch(config.options.size())
    {
      case 0: {
        node->create_parameter(ossia::val_type::IMPULSE);
        break;
      }
      // FIXME when we have only one parameter, simplify things?
      // We need to store the option name somewhere though
      // case 1: {
      //   auto& opt = config.options[0];
      //   if(opt.type == "static-text")
      //     node->create_parameter(ossia::val_type::IMPULSE);
      //   else
      //     setup_option_parameter(opt, node);
      //   break;
      // }
      default: {
        node->create_parameter(ossia::val_type::IMPULSE);
        for(auto& opt : config.options)
        {
          if(opt.type == "static-text")
            continue;

          auto cld = node->create_child(opt.id.toStdString());
          setup_option_parameter(opt, cld);
        }
      }
    }
  }

  void setup_option_parameter(
      const bitfocus::module_data::config_field& opt, ossia::net::node_base* cld)
  {
    ossia::net::set_description(*cld, opt.label.toStdString());
    if(opt.type == "textinput" || opt.type == "bonjourdevice")
    {
      auto p = cld->create_parameter(ossia::val_type::STRING);
      p->set_value(opt.default_value.toString().toStdString());
    }
    else if(opt.type == "number")
    {
      // FIXME int
      auto p = cld->create_parameter(ossia::val_type::FLOAT);
      p->set_value(opt.default_value.toDouble());
      auto dom = ossia::domain_base<float>{};
      dom.min = opt.min.toDouble();
      dom.max = opt.max.toDouble();
      p->set_domain(std::move(dom));
    }
    else if(opt.type == "checkbox" || opt.type == "boolean")
    {
      auto p = cld->create_parameter(ossia::val_type::BOOL);
      p->set_value(opt.default_value.toBool());
    }
    else if(opt.type == "choices" || opt.type == "dropdown")
    {
      auto p = cld->create_parameter(ossia::val_type::STRING);
      auto dom = ossia::domain_base<std::string>{};
      for(const auto& choice : opt.choices)
        dom.values.push_back(choice.id.toStdString());
      p->set_value(opt.default_value.toString().toStdString());
      p->set_domain(std::move(dom));
    }
  }

  void init_device()
  {
    if(!m_dev)
      return;
    if(nodes.actions || nodes.feedbacks || nodes.variables)
      return;

    auto& dev = *m_dev;
    // Start creating the tree
    auto& m = m_rc->model();
    nodes.actions
        = m.actions.empty() ? nullptr : dev.get_root_node().create_child("action");
    // nodes.presets
    //     = m.presets.empty() ? nullptr : dev.get_root_node().create_child("presets");
    nodes.feedbacks
        = m.feedbacks.empty() ? nullptr : dev.get_root_node().create_child("feedback");
    nodes.variables
        = m.variables.empty() ? nullptr : dev.get_root_node().create_child("variable");

    for(auto& v : m.actions)
    {
      auto node = nodes.actions->create_child(v.first.toStdString());
      setup_node(v.second, node);
    }

    // for(auto& v : m.presets)
    // {
    //   auto node = nodes.presets->create_child(v.first.toStdString());
    //   ossia::net::set_description(*node, v.second.name.toStdString());
    //   auto param = node->create_parameter(ossia::val_type::IMPULSE);
    // }

    std::map<QString, bitfocus::module_data::feedback_instance> fb_instances;
    for(auto& v : m.feedbacks)
    {
      auto node = nodes.feedbacks->create_child(v.first.toStdString());
      setup_node(v.second, node);
      if(auto param = node->get_parameter())
        m_feedbacks_recv[v.first] = param;

      // Create a feedback instance to subscribe
      bitfocus::module_data::feedback_instance inst;
      inst.id = v.first;
      inst.controlId = "ossia";
      inst.definitionId = v.first;
      fb_instances[v.first] = std::move(inst);
    }

    // Subscribe to all feedbacks
    if(!fb_instances.empty())
    {
      QMetaObject::invokeMethod(
          m_rc.get(), [rc = m_rc, fb = std::move(fb_instances)]() mutable {
        rc->updateFeedbacks(fb);
      });
    }

    for(auto& v : m.variables)
    {
      auto node = nodes.variables->create_child(v.first.toStdString());
      ossia::net::set_description(*node, v.second.name.toStdString());
      auto val = ossia::qt::qt_to_ossia{}(v.second.value);

      if(val.get_type() != ossia::val_type::NONE)
      {
        auto param = node->create_parameter(val.get_type());
        param->set_value(val);
        m_variables_send[param] = v.first;
        m_variables_recv[v.first] = param;
      }
    }
  }

  void set_device(ossia::net::device_base& dev) override
  {
    m_dev = &dev;
    init_device();
  }

  std::shared_ptr<bitfocus::module_handler> m_rc;
  ossia::net::network_context_ptr m_context;
  ossia::net::device_base* m_dev{};
  struct
  {
    ossia::net::node_base* actions{};
    // ossia::net::node_base* presets{};
    ossia::net::node_base* feedbacks{};
    ossia::net::node_base* variables{};
  } nodes;

  ossia::flat_map<ossia::net::parameter_base*, QString> m_actions;
  // ossia::flat_map<ossia::net::parameter_base*, QString> m_presets;
  ossia::flat_map<QString, ossia::net::parameter_base*> m_variables_recv;
  ossia::flat_map<ossia::net::parameter_base*, QString> m_variables_send;
  ossia::flat_map<QString, ossia::net::parameter_base*> m_feedbacks_recv;
};
}
namespace Protocols
{

BitfocusDevice::BitfocusDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = true;
  m_capas.canLearn = false;
  m_capas.hasCallbacks = false;
}

bool BitfocusDevice::reconnect()
{
  disconnect();

  try
  {
    BitfocusSpecificSettings stgs
        = settings().deviceSpecificSettings.value<BitfocusSpecificSettings>();

    auto conf = bitfocus::module_configuration{};
    {
      if(!stgs.product.isEmpty())
      {
        conf["product"] = stgs.product;
      }
      for(auto& [k, v] : stgs.configuration)
      {
        conf[k] = v.apply(ossia::qt::ossia_to_qvariant{});
      }
    }

    if(!stgs.handler)
    {
      stgs.handler = std::make_shared<bitfocus::module_handler>(
          stgs.path, stgs.entrypoint, stgs.nodeVersion, stgs.apiVersion,
          std::move(conf));
      m_settings.deviceSpecificSettings = QVariant::fromValue(stgs);
    }

    stgs.handler->afterRegistration(
        [name = stgs.name, conf, h = std::weak_ptr{stgs.handler}] {
      if(auto handler = h.lock())
      {
        handler->updateConfigAndLabel(name, conf);
      }
    });

    const auto& name = settings().name.toStdString();
    if(auto proto = std::make_unique<ossia::net::bitfocus_protocol>(stgs.handler, m_ctx))
    {
      auto pproto = proto.get();
      m_dev = std::make_shared<ossia::net::generic_device>(std::move(proto), name);

      stgs.handler->afterRegistration([dev = std::weak_ptr{m_dev}, pproto] {
        if(auto d = dev.lock())
          pproto->init_device();
      });
      deviceChanged(nullptr, m_dev.get());
      setLogging_impl(Device::get_cur_logging(isLogging()));
    }
    else
    {
      qDebug() << "Could not create Bitfocus protocol";
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "Bitfocus Protocol error: " << e.what();
  }
  catch(...)
  {
    SCORE_TODO;
  }
  return connected();
}

bool BitfocusDevice::isLearning() const
{
  /*
  auto& proto = static_cast<ossia::net::bitfocus5_protocol&>(m_dev->get_protocol());
  return proto.learning();
  */
  return false;
}

void BitfocusDevice::setLearning(bool b)
{
  /*
  if(!m_dev)
    return;
  auto& proto = static_cast<ossia::net::bitfocus5_protocol&>(m_dev->get_protocol());
  auto& dev = *m_dev;
  if(b)
  {
    dev.on_node_created.connect<&DeviceInterface::nodeCreated>((DeviceInterface*)this);
    dev.on_node_removing.connect<&DeviceInterface::nodeRemoving>((DeviceInterface*)this);
    dev.on_node_renamed.connect<&DeviceInterface::nodeRenamed>((DeviceInterface*)this);
    dev.on_parameter_created.connect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.connect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }
  else
  {
    dev.on_node_created.disconnect<&DeviceInterface::nodeCreated>(
        (DeviceInterface*)this);
    dev.on_node_removing.disconnect<&DeviceInterface::nodeRemoving>(
        (DeviceInterface*)this);
    dev.on_node_renamed.disconnect<&DeviceInterface::nodeRenamed>(
        (DeviceInterface*)this);
    dev.on_parameter_created.disconnect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.disconnect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }

  proto.set_learning(b);*/
}
}
