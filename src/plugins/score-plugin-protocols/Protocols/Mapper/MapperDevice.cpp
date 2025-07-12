// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia-qt/qml_protocols.hpp>
#if __has_include(<QQmlEngine>)
#include <Process/Script/ScriptWidget.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/LibraryDeviceEnumerator.hpp>
#include <Protocols/Mapper/MapperDevice.hpp>

#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/Layout.hpp>
#include <score/widgets/TextLabel.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/node_visitor.hpp>
#include <ossia/network/generic/wrapped_parameter.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/js_utilities.hpp>
#include <ossia-qt/qml_engine_functions.hpp>

#include <QCodeEditor>
#include <QLineEdit>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QUrl>

#include <wobjectimpl.h>

#include <verdigris>
namespace ossia::net
{
class observable_device_roots final : public QObject
{
  W_OBJECT(observable_device_roots)
public:
  observable_device_roots(Device::DeviceList& devices)
  {
    devices.apply([this](auto& dev) { on_deviceAdded(&dev); });
    con(devices, &Device::DeviceList::deviceAdded, this,
        &observable_device_roots::on_deviceAdded);

    con(devices, &Device::DeviceList::deviceRemoved, this,
        &observable_device_roots::on_deviceRemoved);
  }

  void on_deviceAdded(const Device::DeviceInterface* d)
  {
    connect(
        d, &Device::DeviceInterface::deviceChanged, this,
        &observable_device_roots::on_deviceAddedCallback, Qt::UniqueConnection);
    if(auto dev = d->getDevice())
      m_devices.push_back(dev);

    QTimer::singleShot(1, this, [this] { rootsChanged(roots()); });
  }

  void
  on_deviceAddedCallback(ossia::net::device_base* oldd, ossia::net::device_base* newd)
  {
    ossia::remove_erase(m_devices, oldd);
    if(newd)
      m_devices.push_back(newd);

    QTimer::singleShot(1, this, [this] { rootsChanged(roots()); });
  }

  void on_deviceRemoved(const Device::DeviceInterface* d)
  {
    disconnect(
        d, &Device::DeviceInterface::deviceChanged, this,
        &observable_device_roots::on_deviceAddedCallback);
    ossia::remove_erase(m_devices, d->getDevice());

    QTimer::singleShot(1, this, [this] { rootsChanged(roots()); });
  }

  void rootsChanged(std::vector<ossia::net::node_base*> a) W_SIGNAL(rootsChanged, a);

  std::vector<ossia::net::node_base*> roots() const noexcept
  {
    std::vector<ossia::net::node_base*> r;
    r.reserve(m_devices.size());
    for(auto d : m_devices)
      r.push_back(&d->get_root_node());
    return r;
  }
  const auto& devices() const noexcept { return m_devices; }

private:
  std::vector<ossia::net::device_base*> m_devices;
};

ossia::net::parameter_base* find_parameter(
    ossia::net::node_base& root, const std::vector<ossia::net::node_base*>& roots,
    const QString& str)
{
  auto d = str.indexOf(':');
  if(d == -1)
  {
    // Address looks like '/foo/bar'
    // Try to find automatically in current root
    if(auto node = find_node(root, str.toStdString()))
    {
      return node->get_parameter();
    }
  }

  // Split in devices
  auto dev
      = ossia::find_if(roots, [devname = str.mid(0, d).toStdString()](const auto& dev) {
          return dev && dev->get_name() == devname;
        });

  if(dev != roots.end())
  {
    if(d == str.size() - 1)
    {
      return (*dev)->get_parameter();
    }

    if(auto node = find_node(**dev, str.mid(d + 1).toStdString()))
    {
      return node->get_parameter();
    }
  }

  // TODO handle path traversals... foo:/bar/*, etc
  return nullptr;
}

static ossia::small_vector<ossia::net::parameter_base*, 4> setup_sources(
    const QJSValue& jsval, ossia::net::node_base& self,
    const std::vector<ossia::net::node_base*>& roots)
{
  ossia::small_vector<ossia::net::parameter_base*, 4> res;
  if(jsval.isString())
  {
    res.push_back(find_parameter(self, roots, jsval.toString()));
  }
  else if(jsval.isArray())
  {
    QJSValueIterator it(jsval);
    while(it.hasNext())
    {
      it.next();
      if(const auto& val = it.value(); val.isString())
      {
        res.push_back(find_parameter(self, roots, val.toString()));
      }
      else
      {
        res.push_back(nullptr);
      }
    }
  }
  return res;
}

static void apply_reply(
    ossia::net::node_base& self, const std::vector<ossia::net::node_base*>& roots,
    const QJSValue& arr)
{
  // should be an array of { address, value } objects
  QJSValueIterator it(arr);
  while(it.hasNext())
  {
    it.next();
    auto val = it.value();
    auto addr = val.property("address");
    auto v = val.property("value");
    if(addr.isString() && !v.isNull())
    {
      auto addr_txt = addr.toString().toStdString();
      if(auto p = find_parameter(self, roots, addr.toString()))
        p->push_value(qt::value_from_js(p->value(), v));
    }
  }
}

static ossia::small_vector<ossia::value, 4> apply_reply(const QJSValue& arr)
{
  ossia::small_vector<ossia::value, 4> res;
  if(arr.isArray())
  {
    QJSValueIterator it(arr);
    while(it.hasNext())
    {
      it.next();
      res.push_back(ossia::qt::value_from_js(it.value()));
    }
  }
  return res;
}

struct mapper_parameter_data_base
{
  mapper_parameter_data_base() = default;
  mapper_parameter_data_base(const mapper_parameter_data_base&) = delete;
  mapper_parameter_data_base(mapper_parameter_data_base&& other)
      : bind{std::move(other.bind)}
      , read{std::move(other.read)}
      , write{std::move(other.write)}
      , interval{std::move(other.interval)}
      , source{std::move(other.source)}
  {
  }

  mapper_parameter_data_base& operator=(const mapper_parameter_data_base&) = delete;
  mapper_parameter_data_base& operator=(mapper_parameter_data_base&&) = delete;

  mapper_parameter_data_base(const QJSValue& val)
      : bind{val.property("bind")}
      , read{val.property("read")}
      , write{val.property("write")}
  {
    if(auto v = val.property("interval"); v.isNumber())
    {
      interval = v.toNumber();
    }
  }

  bool valid(const QJSValue& val) const noexcept
  {
    return !val.isUndefined() && !val.isNull();
  }
  bool valid() const noexcept
  {
    return valid(bind) || valid(write) || (interval && valid(read));
  }

  QJSValue bind;
  QJSValue read;
  QJSValue write;
  std::optional<double> interval;
  ossia::small_vector<ossia::net::parameter_base*, 4> source{};
  std::mutex source_lock;
};

struct mapper_parameter_data final
    : public parameter_data
    , public mapper_parameter_data_base
{
  using base_data_type = mapper_parameter_data_base;
  mapper_parameter_data() = default;
  mapper_parameter_data(const mapper_parameter_data&) = delete;
  mapper_parameter_data(mapper_parameter_data&&) = default;
  mapper_parameter_data& operator=(const mapper_parameter_data&) = delete;
  mapper_parameter_data& operator=(mapper_parameter_data&&) = delete;

  mapper_parameter_data(const std::string& name)
      : parameter_data{name}
  {
  }

  mapper_parameter_data(const QJSValue& val)
      : parameter_data{ossia::qt::make_parameter_data(val)}
      , mapper_parameter_data_base{val}
  {
  }
};

class mapper_protocol;
struct mapper_parameter final
    : wrapped_parameter<mapper_parameter_data>
    , Nano::Observer
{
public:
  using wrapped_parameter<mapper_parameter_data>::wrapped_parameter;
  ~mapper_parameter() override
  {
    for(auto& n : callbacks)
    {
      n.first->about_to_be_deleted.disconnect<&mapper_parameter::on_sourceRemoved>(
          *this);
      if(auto p = n.first->get_parameter())
        p->remove_callback(n.second);
    }

    callback_container<value_callback>::callbacks_clear();
  }

  void connect(ossia::net::parameter_base& s, mapper_protocol& proto);

  void on_sourceRemoved(const ossia::net::node_base& s)
  {
    std::lock_guard g{data().source_lock};
    if(auto p = s.get_parameter())
      ossia::remove_erase(data().source, p);

    callbacks.erase(&s);
  }

  struct callback_stopper
  {
    mapper_parameter& self;
    callback_stopper(mapper_parameter& self)
        : self{self}
    {
      self.m_stop_callbacks = true;
    }
    ~callback_stopper() { self.m_stop_callbacks = false; }
  };

  callback_stopper stop_callbacks() { return *this; }
  std::atomic_bool m_stop_callbacks = false;
  ossia::hash_map<const ossia::net::node_base*, ossia::net::parameter_base::iterator>
      callbacks;
};
using mapper_node = ossia::net::wrapped_node<mapper_parameter_data, mapper_parameter>;

class mapper_protocol final
    : public QObject
    , public ossia::net::protocol_base
{
  W_OBJECT(mapper_protocol)
public:
  mapper_protocol(
      const QByteArray& code, ossia::net::network_context_ptr ctx,
      Device::DeviceList& roots)
      : protocol_base{flags{}}
      , m_thread{std::make_shared<QThread>()}
      , m_context{ctx}
      , m_code{code}
      , m_devices{roots}
      , m_roots{m_devices.roots()}
  {
    this->moveToThread(m_thread.get());
    m_thread->start();
    m_hasInit++;
    QMetaObject::invokeMethod(this, &mapper_protocol::init_engine, Qt::QueuedConnection);
  }

  ~mapper_protocol() override
  {
    if(m_engine)
    {
      stop();
    }
  }

  void stop() override
  {
    // Necessary for the case where we're quickly redoing a full undo stack
    // which creates and then deletes the Mapper.
    // - otherwise there's a race between this and init_engine - we have to wait
    // for init_engine to complete so that we can delete everything safely, as we cannot
    // delete m_engine on another thread than our m_thread.
    while(m_hasInit > 0)
      std::this_thread::yield();

    auto engine = m_engine.load();
    auto comp = m_component.load();
    m_engine = nullptr;
    m_component = nullptr;
    SCORE_ASSERT(m_thread->isRunning());

    QMetaObject::invokeMethod(this, [this, comp, engine, t = QThread::currentThread()] {
      delete comp;
      delete engine;
      if(t)
        this->moveToThread(t);

      m_thread->exit();
    }, Qt::QueuedConnection);

    m_thread->wait();
  }

  void teardown_engine(QThread* t) { }

  void init_engine()
  {
    m_engine = new QQmlEngine{};
    m_component = new QQmlComponent{m_engine};

    auto device_obj = new ossia::qt::qml_device_engine_functions{
        {}, [](ossia::net::parameter_base& param, const ossia::value_port& v) {
      if(v.get_data().empty())
        return;
      auto& last = v.get_data().back().value;
      param.push_value(last);
    }, m_engine};
    device_obj->setDevice(m_device);
    for(auto dev : m_devices.devices())
      device_obj->devices.push_back(dev);

    auto protocols_obj = new ossia::qt::qml_protocols{this->m_context, this};

    auto ctx = m_engine.load()->rootContext();
    ctx->setContextProperty("Device", device_obj);
    ctx->setContextProperty("Protocols", protocols_obj);

    QObject::connect(
        this, &mapper_protocol::sig_push, this, &mapper_protocol::slot_push);
    QObject::connect(
        this, &mapper_protocol::sig_recv, this, &mapper_protocol::slot_recv);
    con(m_devices, &observable_device_roots::rootsChanged, this,
        [this, device_obj](std::vector<ossia::net::node_base*> r) {
      ossia::qt::qml_device_cache cache;
      for(auto node : r)
        cache.push_back(&node->get_device());
      ossia::remove_duplicates(cache);
      device_obj->devices = cache;
      m_roots = std::move(r);
      reset_tree();
    }, Qt::QueuedConnection);

    QObject::connect(
        m_component, &QQmlComponent::statusChanged, this,
        [this](QQmlComponent::Status status) {
      if(!m_device)
        return;

      switch(status)
      {
        case QQmlComponent::Status::Ready: {
          if((m_object = m_component.load()->create()))
          {
            m_object->setParent(m_engine.load()->rootContext());

            QVariant ret;
            QMetaObject::invokeMethod(
                m_object, "createTree", Q_RETURN_ARG(QVariant, ret));
            qt::create_device<ossia::net::device_base, mapper_node, mapper_protocol>(
                *m_device, ret.value<QJSValue>());
            reset_tree();
          }
          else
          {
            qDebug() << "Mapper: could not create object";
          }
          return;
        }
        case QQmlComponent::Status::Loading:
          return;
        case QQmlComponent::Status::Null:
        case QQmlComponent::Status::Error:
          qDebug() << m_component.load()->errorString();
          return;
      }
    });

    m_hasInit--;
  }

  void sig_push(mapper_parameter* p, const ossia::value& v) W_SIGNAL(sig_push, p, v);
  void
  sig_recv(mapper_parameter* p, ossia::net::parameter_base* s, const ossia::value& v)
      W_SIGNAL(sig_recv, p, s, v);

  static bool isAddressValueArray(const QJSValue& v)
  {
    if(!v.isArray())
      return false;
    const int len = v.property("length").toInt();
    if(len == 0)
      return true;
    for(int i = 0; i < len; i++)
    {
      if(const auto& obj = v.property(i); obj.isObject())
      {
        if(obj.hasProperty("address") && obj.hasProperty("value"))
          continue;
      }
      else
      {
        return false;
      }
    }
    return true;
  }
  void slot_push(mapper_parameter* param, const ossia::value& v)
  {
    auto& addr = *param;
    auto& dat = addr.data();
    auto cb = param->stop_callbacks();

    bool write = dat.write.isCallable();
    bool bound = dat.bind.isString() || dat.bind.isArray();
    if(!write && bound)
    {
      std::lock_guard g{dat.source_lock};
      for(auto p : dat.source)
      {
        if(p)
        {
          p->push_value(v);
        }
      }
    }
    else if(write)
    {
      auto res = dat.write.call({qt::value_to_js_value(v, *m_engine)});
      if(bound)
      {
        if(res.isArray())
        {
          if(isAddressValueArray(res))
          {
            std::lock_guard l{m_rootLock};
            apply_reply(m_device->get_root_node(), m_roots, res);
          }
          else
          {
            const auto r = apply_reply(res);
            auto cb = param->stop_callbacks();
            std::lock_guard g{dat.source_lock};
            auto N = std::min(r.size(), dat.source.size());
            for(std::size_t i = 0; i < N; i++)
            {
              if(r[i].valid() && dat.source[i])
              {
                dat.source[i]->push_value(r[i]);
              }
            }
          }
        }
        else
        {
          const auto val = ossia::qt::value_from_js(res);
          std::lock_guard g{dat.source_lock};
          for(auto p : dat.source)
          {
            if(p)
            {
              p->push_value(val);
            }
          }
        }
      }
      else
      {
        if(res.isArray())
        {
          std::lock_guard l{m_rootLock};
          apply_reply(m_device->get_root_node(), m_roots, res);
        }
      }
    }
  }

  void
  slot_recv(mapper_parameter* p, ossia::net::parameter_base* s, const ossia::value& v)
  {
    if(!p->data().read.isCallable())
    {
      p->push_value(v);
    }
    else
    {
      auto res = p->data().read.call(
          {QString::fromStdString(s->get_node().osc_address()),
           qt::value_to_js_value(v, *m_engine)});

      if(res.isArray())
      {
        if(res.property(0).isObject())
        {
          std::lock_guard l{m_rootLock};
          apply_reply(m_device->get_root_node(), m_roots, res);
        }
        else
        {
          p->push_value(qt::value_from_js(std::move(res)));
        }
      }
      else
      {
        p->push_value(qt::value_from_js(std::move(res)));
      }
    }
  }

  static mapper_parameter_data read_data(const QJSValue& js) { return js; }

private:
  void reset_tree()
  {
    // Initialize the roots
    if(!m_device)
      return;

    {
      std::lock_guard g{m_timersLock};
      for(auto [timer, ptr] : m_timers)
      {
        killTimer(timer);
      }
      m_timers.clear();
    }

    std::lock_guard l{m_rootLock};

    ossia::net::visit_parameters(
        m_device->get_root_node(), [&](auto& root, auto& param) {
          mapper_parameter& p = (mapper_parameter&)param;
          mapper_parameter_data_base& data = p.data();

          if(data.valid(data.bind))
          {
            std::lock_guard g{data.source_lock};
            data.source = setup_sources(data.bind, m_device->get_root_node(), m_roots);

            for(auto s : data.source)
            {
              if(s)
              {
                p.connect(*s, *this);
              }
            }
          }
          else if(data.valid(data.read) && data.interval)
          {
            double msecs = *data.interval;
            int timer_id = startTimer(msecs, Qt::PreciseTimer);

            std::lock_guard g{m_timersLock};
            m_timers[timer_id] = &p;
          }
        });
  }

  void timerEvent(QTimerEvent* ev) override
  {
    if(auto it = m_timers.find(ev->timerId()); it != m_timers.end())
    {
      if(auto p = it->second; p && p->data().read.isCallable())
      {
        auto v = qt::value_from_js(p->data().read.call({}));
        if(v != p->value())
          p->set_value(v);
      }
    }
  }

  bool pull(ossia::net::parameter_base&) override
  {
    // TODO
    return false;
  }

  bool
  push(const ossia::net::parameter_base& parameter_base, const ossia::value& v) override
  {
    sig_push((mapper_parameter*)&parameter_base, v);
    return true;
  }

  bool push_raw(const full_parameter_data& parameter_base) override { return false; }

  bool observe(ossia::net::parameter_base&, bool b) override { return false; }

  bool update(ossia::net::node_base& node_base) override { return true; }

  void set_device(device_base& dev) override
  {
    m_device = &dev;
    ossia::qt::run_async(this, [this] {
      if(m_component)
        m_component.load()->setData(m_code, QUrl{});
    });
  }

private:
  std::shared_ptr<QThread> m_thread;
  std::atomic_int m_hasInit = 0;
  std::atomic<QQmlEngine*> m_engine{};
  std::atomic<QQmlComponent*> m_component{};

  ossia::net::network_context_ptr m_context{};
  ossia::net::device_base* m_device{};
  QObject* m_object{};
  QByteArray m_code;

  observable_device_roots m_devices;

  std::mutex m_rootLock;
  std::vector<ossia::net::node_base*> m_roots;

  std::mutex m_timersLock;
  ossia::hash_map<int, mapper_parameter*> m_timers;
};

using mapper_device = ossia::net::wrapped_device<mapper_node, mapper_protocol>;

void mapper_parameter::connect(parameter_base& s, mapper_protocol& proto)
{
  set_value(s.value());
  s.get_node().about_to_be_deleted.connect<&mapper_parameter::on_sourceRemoved>(*this);
  // TODO handle parameter removal from device -> some hash_map
  auto it = callbacks.find(&s.get_node());
  if(it == callbacks.end())
  {
    QPointer<mapper_protocol> proto_ptr = &proto;
    callbacks[&s.get_node()]
        = s.add_callback([this, param = &s, proto_ptr](const ossia::value& v) {
            if(!this->m_stop_callbacks)
            {
              SCORE_ASSERT(proto_ptr);
              proto_ptr->sig_recv(this, param, v);
            }
          });
  }
  else
  {
    qDebug() << "Warning ! callback for" << s.get_node().osc_address().c_str()
             << "already exists";
    return;
  }
}

}

namespace Protocols
{

class MapperDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(MapperDevice)

public:
  MapperDevice(
      const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx,
      const score::DocumentContext& cctx)
      : OwningDeviceInterface{settings}
      , net_context{ctx}
      , context{cctx}
      , m_list{}
  {
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canSerialize = false;
    m_capas.canRenameNode = false;
    m_capas.canSetProperties = false;
  }

  bool reconnect() override
  {
    disconnect();

    auto devlist = devices();
    if(!devlist)
      return false;

    try
    {
      const auto& stgs
          = settings().deviceSpecificSettings.value<MapperSpecificSettings>();

      auto proto = std::make_unique<ossia::net::mapper_protocol>(
          stgs.text.toUtf8(), this->net_context, *devlist);
      auto nm = settings().name.toStdString();
      m_dev = std::make_unique<ossia::net::mapper_device>(
          static_cast<std::unique_ptr<ossia::net::mapper_protocol>&&>(proto), nm);

      deviceChanged(nullptr, m_dev.get());

      enableCallbacks();

      setLogging_impl(Device::get_cur_logging(isLogging()));
    }
    catch(std::exception& e)
    {
      qDebug() << "Could not connect: " << e.what();
    }
    catch(...)
    {
      // TODO save the reason of the non-connection.
    }

    return connected();
  }

  ~MapperDevice() override { }

private:
  Device::DeviceList* devices()
  {
    if(m_list)
      return m_list;

    auto plug = context.findPlugin<Explorer::DeviceDocumentPlugin>(); // list()
    if(plug)
      m_list = &plug->list();

    return m_list;
  }
  ossia::net::network_context_ptr net_context;
  const score::DocumentContext& context;
  Device::DeviceList* m_list{};
};

MapperProtocolFactory::~MapperProtocolFactory() { }
QString MapperProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Mapper");
}

QString MapperProtocolFactory::category() const noexcept
{
  return StandardCategories::util;
}

QUrl MapperProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/mapper-device.html");
}
Device::DeviceEnumerators
MapperProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  auto library_enumerator = new LibraryDeviceEnumerator{
      "Ossia.Mapper",
      {"qml"},
      MapperProtocolFactory::static_concreteKey(),
      [](const QByteArray& arr) {
    return QVariant::fromValue(MapperSpecificSettings{arr});
      },
      ctx};

  return {{"Library", library_enumerator}};
}

Device::DeviceInterface* MapperProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new MapperDevice{settings, plugin.networkContext(), ctx};
}

const Device::DeviceSettings& MapperProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Mapper";
    MapperSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* MapperProtocolFactory::makeSettingsWidget()
{
  return new MapperProtocolSettingsWidget;
}

QVariant
MapperProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MapperSpecificSettings>(visitor);
}

void MapperProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MapperSpecificSettings>(data, visitor);
}

bool MapperProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

MapperProtocolSettingsWidget::MapperProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  QLabel* deviceNameLabel = new TextLabel(tr("Name"), this);
  m_name = new QLineEdit;

  m_codeEdit = Process::createScriptWidget("JS");

  QGridLayout* gLayout = new QGridLayout;

  gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  gLayout->addWidget(m_name, 0, 1, 1, 1);
  gLayout->addWidget(m_codeEdit, 3, 0, 1, 2);

  setLayout(gLayout);

  setDefaults();
}

void MapperProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_codeEdit);

  m_name->setText("newDevice");
  m_codeEdit->setPlainText("");
}

Device::DeviceSettings MapperProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_name->text();
  s.protocol = MapperProtocolFactory::static_concreteKey();

  s.deviceSpecificSettings
      = QVariant::fromValue(MapperSpecificSettings{m_codeEdit->toPlainText()});
  return s;
}

void MapperProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
  MapperSpecificSettings specific;
  if(settings.deviceSpecificSettings.canConvert<MapperSpecificSettings>())
  {
    specific = settings.deviceSpecificSettings.value<MapperSpecificSettings>();

    m_codeEdit->setPlainText(specific.text);
  }
}
}

template <>
void DataStreamReader::read(const Protocols::MapperSpecificSettings& n)
{
  m_stream << n.text;
}

template <>
void DataStreamWriter::write(Protocols::MapperSpecificSettings& n)
{
  m_stream >> n.text;
}

template <>
void JSONReader::read(const Protocols::MapperSpecificSettings& n)
{
  obj["Text"] = n.text;
}

template <>
void JSONWriter::write(Protocols::MapperSpecificSettings& n)
{
  n.text = obj["Text"].toString();
}

Q_DECLARE_METATYPE(ossia::net::mapper_parameter*)
W_REGISTER_ARGTYPE(ossia::net::mapper_parameter*)
W_OBJECT_IMPL(Protocols::MapperDevice)
W_OBJECT_IMPL(ossia::net::observable_device_roots)
W_OBJECT_IMPL(ossia::net::mapper_protocol)
#endif
