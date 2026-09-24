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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QUrl>

#include <condition_variable>
#include <mutex>

#include <wobjectimpl.h>

#include <verdigris>
namespace ossia::net
{
class observable_device_roots final : public QObject
{
  W_OBJECT(observable_device_roots)
public:
  explicit observable_device_roots(Device::DeviceList& devices)
  {
    devices.apply([this](auto& dev) { on_deviceAdded(&dev); });
    con(devices, &Device::DeviceList::deviceAdded, this,
        &observable_device_roots::on_deviceAdded);

    con(devices, &Device::DeviceList::deviceRemoved, this,
        &observable_device_roots::on_deviceRemoved);
  }
  ~observable_device_roots()
  {
  }

  void on_deviceAdded(const Device::DeviceInterface* d)
  {
    const int n = ++m_updating_index;
    connect(
        d, &Device::DeviceInterface::deviceChanged, this,
        &observable_device_roots::on_deviceAddedCallback, Qt::UniqueConnection);
    if(auto dev = d->getDevice())
    {
      std::lock_guard l{m_devicesLock};
      m_devices.push_back(dev);
      notify_added(dev);
    }

    QTimer::singleShot(1, this, [this, n] {
      rootsChanged(roots(), n);
    });
  }

  void
  on_deviceAddedCallback(ossia::net::device_base* oldd, ossia::net::device_base* newd)
  {
    const int n = ++m_updating_index;
    {
      std::lock_guard l{m_devicesLock};
      notify_removing(oldd);
      ossia::remove_erase(m_devices, oldd);
      if(newd)
      {
        m_devices.push_back(newd);
        notify_added(newd);
      }
    }

    QTimer::singleShot(1, this, [this, n] {
      rootsChanged(roots(), n);
    });
  }

  void on_deviceRemoved(const Device::DeviceInterface* d)
  {
    const int n = ++m_updating_index;
    disconnect(
        d, &Device::DeviceInterface::deviceChanged, this,
        &observable_device_roots::on_deviceAddedCallback);
    {
      std::lock_guard l{m_devicesLock};
      notify_removing(d->getDevice());
      ossia::remove_erase(m_devices, d->getDevice());
    }

    QTimer::singleShot(1, this, [this, n] { rootsChanged(roots(), n); });
  }

  //! Removal must reach the cache while the device is still alive
  //! (rootsChanged is deferred, too late). Called from the mapper thread: the
  //! list is handed over under the same lock as the hooks.
  void set_engine_functions(ossia::qt::qml_engine_functions* f)
  {
    std::lock_guard l{m_devicesLock};
    m_functions = f;
    if(f)
      for(auto dev : m_devices)
        f->addDevice(dev);
  }

  //! Requires m_devicesLock.
  void notify_added(ossia::net::device_base* d)
  {
    if(m_functions)
      m_functions->addDevice(d);
  }

  //! Requires m_devicesLock.
  void notify_removing(ossia::net::device_base* d)
  {
    if(!d)
      return;
    if(m_functions)
      m_functions->removeDevice(d);
  }

  void rootsChanged(std::vector<ossia::net::node_base*> a, int64_t i)
      W_SIGNAL(rootsChanged, a, i);

  std::vector<ossia::net::node_base*> roots() const noexcept
  {
    std::vector<ossia::net::node_base*> r;
    r.reserve(m_devices.size());
    for(auto d : m_devices) {
      r.push_back(&d->get_root_node());
    }
    return r;
  }

  std::atomic_int64_t m_updating_index = 0;

private:
  //! m_devices is only written on the main thread, which reads it unlocked;
  //! the mapper thread reaches it only through set_engine_functions().
  std::mutex m_devicesLock;
  ossia::qt::qml_engine_functions* m_functions{};
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
    const std::vector<QString>& bind, ossia::net::node_base& self,
    const std::vector<ossia::net::node_base*>& roots)
{
  ossia::small_vector<ossia::net::parameter_base*, 4> res;
  for(const auto& address : bind)
  {
    if(!address.isNull())
      res.push_back(find_parameter(self, roots, address));
    else
      res.push_back(nullptr);
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

//! What a parameter of the script's tree needs on the main thread. It holds no
//! QJSValue: the script's read / write functions stay in the protocol's
//! engine-side table, under `id`, and are only ever touched on the engine thread.
struct mapper_parameter_data_base
{
  mapper_parameter_data_base() = default;
  mapper_parameter_data_base(const mapper_parameter_data_base&) = delete;
  mapper_parameter_data_base(mapper_parameter_data_base&& other)
      : bind{std::move(other.bind)}
      , interval{std::move(other.interval)}
      , id{other.id}
      , has_bind{other.has_bind}
      , has_read{other.has_read}
      , source{std::move(other.source)}
  {
  }

  mapper_parameter_data_base& operator=(const mapper_parameter_data_base&) = delete;
  mapper_parameter_data_base& operator=(mapper_parameter_data_base&&) = delete;

  bool valid() const noexcept
  {
    return true;
  }

  //! The addresses of "bind", a null string standing for an entry that was not
  //! one.
  std::vector<QString> bind;
  std::optional<double> interval;
  //! Key of the script callbacks in mapper_protocol::m_scripts; -1 for the
  //! parameters the script did not describe (root, Device.addNode()).
  int id{-1};
  bool has_bind{};
  bool has_read{};

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

  mapper_parameter_data(parameter_data&& data)
      : parameter_data{std::move(data)}
  {
  }
};

class mapper_protocol;
struct mapper_parameter final
    : wrapped_parameter<mapper_parameter_data>
    , Nano::Observer
{
public:
  mapper_parameter(mapper_parameter_data&& data, ossia::net::node_base& node);
  ~mapper_parameter() override;

  void connect(ossia::net::parameter_base& s, mapper_protocol& proto);

  void on_sourceRemoved(const ossia::net::node_base& s)
  {
    std::lock_guard g{data().source_lock};
    if(auto p = s.get_parameter())
      ossia::remove_erase(data().source, p);

    callbacks.erase(&s);
  }

  ossia::hash_map<const ossia::net::node_base*, ossia::net::parameter_base::iterator>
      callbacks;

private:
  mapper_protocol* m_protocol{};
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
    // Both live on the main thread: the bindings to the other devices' trees
    // are made there, along with the tree itself.
    con(m_devices, &observable_device_roots::rootsChanged, &m_mainContext,
        [this](std::vector<ossia::net::node_base*> r, int64_t n) {
      if(m_devices.m_updating_index != n)
        return;

      // `r` was computed right before this direct call, on this thread: the
      // nodes are still alive.
      {
        std::lock_guard l{m_rootLock};
        if(!m_treeAccess)
          return;
        m_roots = std::move(r);
      }
      reset_tree();
    });

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
    // Also a no-op when the owning device already cut the access in
    // disconnect(), before clearing our tree.
    disable_device_access();

    auto engine = m_engine.load();
    auto comp = m_component.load();
    m_engine = nullptr;
    m_component = nullptr;
    SCORE_ASSERT(m_thread->isRunning());

    QMetaObject::invokeMethod(this, [this, comp, engine, t = QThread::currentThread()] {
      // The script's functions belong to the engine's thread.
      m_scripts.clear();
      delete comp;
      delete engine;
      if(t)
        this->moveToThread(t);

      m_thread->exit();
    }, Qt::QueuedConnection);

    m_thread->wait();
  }

  //! Definitive: from now on Device.read/write in the script are no-ops.
  //! Must run before any tree the script may have resolved addresses in is
  //! cleared: the engine functions cache raw parameter pointers, and the
  //! removal hooks only drop them once the tree is already gone.
  void disable_device_access()
  {
    // init_engine registers the engine functions: it must have run for the
    // unregistration below to stick.
    while(m_hasInit > 0)
      std::this_thread::yield();

    // The engine thread may be waiting for a tree that will not be built.
    finish_tree_build(true);

    // Main thread, like the removal hooks. disable() waits for any script
    // inside Device.read/write and no-ops later calls.
    m_devices.set_engine_functions(nullptr);
    if(auto* fun = m_deviceFunctions.exchange(nullptr))
      fun->disable();

    // Waits for a reply being applied; later trees are not built.
    std::lock_guard l{m_rootLock};
    m_treeAccess = false;
    m_roots.clear();
  }

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
    }, *m_engine, m_engine};
    // No setDevice(m_device): set_device() writes it concurrently on the main
    // thread, and the lambda it posts always runs after us.
    m_deviceFunctions = device_obj;
    // Device.addNode/removeNode: the tree only changes on the main thread.
    // m_deviceFunctions is only cleared there, by disable_device_access(),
    // before stop() deletes device_obj on this thread.
    device_obj->setTreeEditor([this](std::function<void()> edit) {
      ossia::qt::run_async(&m_mainContext, [this, edit = std::move(edit)] {
        if(m_deviceFunctions.load())
          edit();
      });
    });
    m_devices.set_engine_functions(device_obj);

    auto protocols_obj = new ossia::qt::qml_protocols{this->m_context, this};

    auto ctx = m_engine.load()->rootContext();
    ctx->setContextProperty("Device", device_obj);
    ctx->setContextProperty("Protocols", protocols_obj);

    QObject::connect(
        this, &mapper_protocol::sig_push, this, &mapper_protocol::slot_push);
    QObject::connect(
        this, &mapper_protocol::sig_recv, this, &mapper_protocol::slot_recv);

    QObject::connect(
        m_component, &QQmlComponent::statusChanged, this,
        [this](QQmlComponent::Status status) {
      if(!m_device)
        return;

      // Queued slots and lambdas can still run on the mapper thread after
      // stop() released the engine and the component from the main thread.
      auto engine = m_engine.load();
      auto comp = m_component.load();
      if(!engine || !comp)
        return;

      switch(status)
      {
        case QQmlComponent::Status::Ready: {
          if((m_object = comp->create()))
          {
            m_object->setParent(engine->rootContext());

            QVariant ret;
            QMetaObject::invokeMethod(
                m_object, "createTree", Q_RETURN_ARG(QVariant, ret));

            // Only the script runs here; the tree is built on the main thread.
            auto tree = read_tree(ret.value<QJSValue>());
            {
              std::lock_guard l{m_treeBuildLock};
              if(m_treeBuildCancelled)
                return;
              m_treeBuildPending = true;
            }
            ossia::qt::run_async(
                &m_mainContext,
                [this, tree = std::move(tree)]() mutable { create_tree(tree); });

            // The script's next Device.write (onOpen...) needs the tree: wait
            // until it is built, or until teardown cancels.
            std::unique_lock l{m_treeBuildLock};
            m_treeBuildDone.wait(l, [this] { return !m_treeBuildPending; });
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
          qDebug() << comp->errorString();
          return;
      }
    });

    m_hasInit--;
  }

  void sig_push(int id, const ossia::value& v) W_SIGNAL(sig_push, id, v);
  void sig_recv(int id, const QString& source, const ossia::value& v)
      W_SIGNAL(sig_recv, id, source, v);

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

  void slot_push(int id, const ossia::value& v)
  {
    auto engine = m_engine.load();
    if(!engine)
      return;

    auto it = m_scripts.find(id);
    if(it == m_scripts.end())
      return;
    auto& script = it->second;
    auto cb = script_stopper{script};

    bool write = script.write.isCallable();
    bool bound = script.bound;
    if(!write && bound)
    {
      push_to_sources(id, v);
    }
    else if(write)
    {
      auto res = script.write.call({qt::value_to_js_value(v, *engine)});
      if(bound)
      {
        if(res.isArray())
        {
          if(isAddressValueArray(res))
          {
            apply_reply_to_roots(res);
          }
          else
          {
            push_to_sources(id, apply_reply(res));
          }
        }
        else
        {
          push_to_sources(id, ossia::qt::value_from_js(res));
        }
      }
      else
      {
        if(res.isArray())
        {
          apply_reply_to_roots(res);
        }
      }
    }
  }

  void slot_recv(int id, const QString& source, const ossia::value& v)
  {
    auto engine = m_engine.load();
    if(!engine)
      return;

    // A source pushed by this very parameter's own script, from slot_push:
    // the callback ran on this thread and called us directly.
    auto it = m_scripts.find(id);
    if(it == m_scripts.end() || it->second.stopped > 0)
      return;
    auto& script = it->second;

    if(!script.read.isCallable())
    {
      with_parameter(id, [&](mapper_parameter& p) { p.push_value(v); });
    }
    else
    {
      auto res = script.read.call({source, qt::value_to_js_value(v, *engine)});

      if(res.isArray() && res.property(0).isObject())
      {
        apply_reply_to_roots(res);
      }
      else
      {
        auto val = qt::value_from_js(std::move(res));
        with_parameter(id, [&](mapper_parameter& p) { p.push_value(val); });
      }
    }
  }

  void register_parameter(mapper_parameter& p, int id)
  {
    std::lock_guard l{m_parametersLock};
    m_parameters[id] = &p;
  }

  void unregister_parameter(int id)
  {
    std::lock_guard l{m_parametersLock};
    m_parameters.erase(id);
  }

private:
  //! Engine thread: what the script gave for one parameter.
  struct mapper_script
  {
    QJSValue read;
    QJSValue write;
    bool bound{};
    //! Set while slot_push runs the parameter's script: what the script
    //! pushes to the parameter's own sources must not come back to it.
    int stopped{};
  };

  struct script_stopper
  {
    mapper_script& self;
    explicit script_stopper(mapper_script& self)
        : self{self}
    {
      ++self.stopped;
    }
    ~script_stopper() { --self.stopped; }
  };

  using tree_description = ossia::qt::deferred_js_node<mapper_parameter_data>;

  //! Engine thread: turns what createTree() returned into plain data, the
  //! script functions going to m_scripts.
  tree_description read_tree(const QJSValue& root)
  {
    m_scripts.clear();

    tree_description res;
    if(!root.isArray())
      return res;

    QJSValueIterator it(root);
    while(it.hasNext())
    {
      it.next();
      read_node(it.value(), res);
    }
    return res;
  }

  void read_node(const QJSValue& js, tree_description& parent)
  {
    mapper_parameter_data data{ossia::qt::make_parameter_data(js)};
    if(data.name.empty())
      return;

    const int id = m_lastScriptId++;
    data.id = id;

    auto& script = m_scripts[id];
    script.read = js.property("read");
    script.write = js.property("write");
    data.has_read = script.read.isCallable();

    if(auto v = js.property("interval"); v.isNumber())
      data.interval = v.toNumber();

    const auto bind = js.property("bind");
    data.has_bind = !bind.isUndefined() && !bind.isNull();
    if(bind.isString())
    {
      data.bind.push_back(bind.toString());
      script.bound = true;
    }
    else if(bind.isArray())
    {
      QJSValueIterator it(bind);
      while(it.hasNext())
      {
        it.next();
        if(const auto& val = it.value(); val.isString())
          data.bind.push_back(val.toString());
        else
          data.bind.push_back(QString{});
      }
      script.bound = true;
    }

    parent.children.push_back(tree_description{std::move(data), {}});

    const QJSValue children = js.property("children");
    if(!children.isArray())
      return;

    auto& node = parent.children.back();
    QJSValueIterator it(children);
    while(it.hasNext())
    {
      it.next();
      read_node(it.value(), node);
    }
  }

  //! Main thread: the whole tree is in place before anyone is told about it.
  void create_tree(tree_description& tree)
  {
    create_tree_impl(tree);
    finish_tree_build(false);
  }

  //! Releases the engine thread waiting in the Ready handler; `cancel` for
  //! good.
  void finish_tree_build(bool cancel)
  {
    {
      std::lock_guard l{m_treeBuildLock};
      m_treeBuildPending = false;
      if(cancel)
        m_treeBuildCancelled = true;
    }
    m_treeBuildDone.notify_all();
  }

  void create_tree_impl(tree_description& tree)
  {
    if(!m_device || !m_treeAccess)
      return;

    std::vector<ossia::net::node_base*> created;
    auto& root = static_cast<mapper_node&>(m_device->get_root_node());
    for(auto& child : tree.children)
      create_node(child, root, created);

    for(auto node : created)
      m_device->on_node_created(*node);

    reset_tree();
  }

  void create_node(
      tree_description& desc, mapper_node& parent,
      std::vector<ossia::net::node_base*>& created)
  {
    auto node = new mapper_node{std::move(desc.data), *m_device, parent};
    parent.add_child(std::unique_ptr<ossia::net::node_base>(node));
    created.push_back(node);

    for(auto& child : desc.children)
      create_node(child, *node, created);
  }

  //! Engine thread. The tree is destroyed on the main thread, which waits for
  //! `f` to be done with the parameter before destroying it.
  template <typename F>
  void with_parameter(int id, F&& f)
  {
    std::lock_guard l{m_parametersLock};
    if(auto it = m_parameters.find(id); it != m_parameters.end())
      f(*it->second);
  }

  void push_to_sources(int id, const ossia::value& v)
  {
    with_parameter(id, [&](mapper_parameter& p) {
      auto& dat = p.data();
      std::lock_guard g{dat.source_lock};
      for(auto s : dat.source)
      {
        if(s)
        {
          s->push_value(v);
        }
      }
    });
  }

  void push_to_sources(int id, const ossia::small_vector<ossia::value, 4>& r)
  {
    with_parameter(id, [&](mapper_parameter& p) {
      auto& dat = p.data();
      std::lock_guard g{dat.source_lock};
      auto N = std::min(r.size(), dat.source.size());
      for(std::size_t i = 0; i < N; i++)
      {
        if(r[i].valid() && dat.source[i])
        {
          dat.source[i]->push_value(r[i]);
        }
      }
    });
  }

  void apply_reply_to_roots(const QJSValue& res)
  {
    std::lock_guard l{m_rootLock};
    if(!m_treeAccess || !m_device)
      return;
    apply_reply(m_device->get_root_node(), m_roots, res);
  }

  //! Main thread. Takes neither m_rootLock, whose fields only change on this
  //! thread, nor a source_lock while it connects: the engine thread takes them
  //! in the other order, from within a source's callbacks.
  void reset_tree()
  {
    // Initialize the roots
    if(!m_device || !m_treeAccess)
      return;

    std::vector<std::pair<int, double>> polled;
    ossia::net::visit_parameters(
        m_device->get_root_node(), [&](auto& root, auto& param) {
      mapper_parameter& p = (mapper_parameter&)param;
      mapper_parameter_data_base& data = p.data();

      if(data.has_bind)
      {
        auto sources = setup_sources(data.bind, m_device->get_root_node(), m_roots);
        for(auto s : sources)
        {
          if(s)
          {
            p.connect(*s, *this);
          }
        }

        std::lock_guard g{data.source_lock};
        data.source = std::move(sources);
      }
      else if(data.has_read && data.interval)
      {
        polled.emplace_back(data.id, *data.interval);
      }
    });

    // Timers belong to the engine's thread.
    ossia::qt::run_async(
        this, [this, polled = std::move(polled)] { restart_timers(polled); });
  }

  //! Engine thread.
  void restart_timers(const std::vector<std::pair<int, double>>& polled)
  {
    if(!m_engine.load())
      return;

    for(auto [timer, id] : m_timers)
    {
      killTimer(timer);
    }
    m_timers.clear();

    for(auto [id, msecs] : polled)
    {
      int timer_id = startTimer(msecs, Qt::PreciseTimer);
      m_timers[timer_id] = id;
    }
  }

  void timerEvent(QTimerEvent* ev) override
  {
    // Same window: the polling timers keep firing until the event loop exits.
    if(!m_engine.load())
      return;

    if(auto it = m_timers.find(ev->timerId()); it != m_timers.end())
    {
      const int id = it->second;
      if(auto s = m_scripts.find(id);
         s != m_scripts.end() && s->second.read.isCallable())
      {
        auto v = qt::value_from_js(s->second.read.call({}));
        with_parameter(id, [&](mapper_parameter& p) {
          if(v != p.value())
            p.set_value(v);
        });
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
    if(const int id = static_cast<const mapper_parameter&>(parameter_base).data().id;
       id >= 0)
      sig_push(id, v);
    return true;
  }

  bool push_raw(const full_parameter_data& parameter_base) override { return false; }

  bool observe(ossia::net::parameter_base&, bool b) override { return false; }

  bool update(ossia::net::node_base& node_base) override { return true; }

  void set_device(device_base& dev) override
  {
    m_device = &dev;
    ossia::qt::run_async(this, [this, &dev] {
      // Runs after init_engine(), which the constructor posted first: the
      // script-facing object knows its owning device before the script loads,
      // so an unqualified address resolves here rather than in a sibling
      // device exposing the same leaf name.
      if(auto* fun = m_deviceFunctions.load())
        fun->setDevice(&dev);
      if(m_component)
        m_component.load()->setData(m_code, QUrl{});
    });
  }

private:
  std::shared_ptr<QThread> m_thread;
  std::atomic_int m_hasInit = 0;
  std::atomic<QQmlEngine*> m_engine{};
  std::atomic<QQmlComponent*> m_component{};
  std::atomic<ossia::qt::qml_device_engine_functions*> m_deviceFunctions{};

  ossia::net::network_context_ptr m_context{};
  ossia::net::device_base* m_device{};
  QObject* m_object{};
  QByteArray m_code;

  observable_device_roots m_devices;
  //! Stays on the main thread, unlike `this`: what has to happen there is
  //! posted to it, and dropped along with it.
  QObject m_mainContext;

  //! Recursive: a reply pushing into this device re-enters slot_push, and so
  //! a new reply, on the same thread.
  std::recursive_mutex m_rootLock;
  std::vector<ossia::net::node_base*> m_roots;
  //! Cleared by disable_device_access(): the tree is being torn down.
  //! m_roots and this are only written on the main thread, under m_rootLock
  //! for the engine thread to read them.
  bool m_treeAccess{true};

  //! The engine thread waits under this, once the script described its tree,
  //! for the main thread to have built it or to be tearing the device down.
  std::mutex m_treeBuildLock;
  std::condition_variable m_treeBuildDone;
  bool m_treeBuildPending{};
  bool m_treeBuildCancelled{};

  //! Parameters of the script's tree, by script id. Written on the main thread
  //! as the tree is built and destroyed, read on the engine thread which may
  //! only reach a parameter under the lock. Recursive for the same re-entrance.
  std::recursive_mutex m_parametersLock;
  ossia::hash_map<int, mapper_parameter*> m_parameters;

  //! Engine thread only: the script functions, destroyed with the engine.
  ossia::hash_map<int, mapper_script> m_scripts;
  int m_lastScriptId{};
  ossia::hash_map<int, int> m_timers;
};

using mapper_device = ossia::net::wrapped_device<mapper_node, mapper_protocol>;

mapper_parameter::mapper_parameter(
    mapper_parameter_data&& data, ossia::net::node_base& node)
    : wrapped_parameter<mapper_parameter_data>{std::move(data), node}
{
  if(const int id = this->data().id; id >= 0)
  {
    m_protocol = &static_cast<mapper_protocol&>(node.get_device().get_protocol());
    m_protocol->register_parameter(*this, id);
  }
}

mapper_parameter::~mapper_parameter()
{
  // First, so that the engine thread is done with us.
  if(m_protocol)
    m_protocol->unregister_parameter(data().id);

  for(auto& n : callbacks)
  {
    n.first->about_to_be_deleted.disconnect<&mapper_parameter::on_sourceRemoved>(*this);
    if(auto p = n.first->get_parameter())
      p->remove_callback(n.second);
  }

  callback_container<value_callback>::callbacks_clear();
}

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
        = s.add_callback([id = data().id, param = &s, proto_ptr](const ossia::value& v) {
      SCORE_ASSERT(proto_ptr);
      // The source is alive while it calls us, not once this reaches the
      // engine thread: its address travels instead.
      proto_ptr->sig_recv(
          id, QString::fromStdString(param->get_node().osc_address()), v);
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

  void disconnect() override
  {
    // Cut the script off before OwningDeviceInterface::disconnect() clears the
    // tree, or a Device.write() on the mapper thread hits freed parameters.
    if(m_owned && m_dev)
    {
      if(auto proto
         = dynamic_cast<ossia::net::mapper_protocol*>(&m_dev->get_protocol()))
        proto->disable_device_access();
    }
    OwningDeviceInterface::disconnect();
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

  m_errorPane = new QPlainTextEdit{this};
  m_errorPane->setReadOnly(true);
  m_errorPane->setMaximumHeight(120);

  m_splitter = new QSplitter{Qt::Vertical, this};
  m_splitter->addWidget(m_codeEdit);
  m_splitter->addWidget(m_errorPane);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);

  auto validateBtn = new QPushButton{tr("Validate"), this};
  connect(validateBtn, &QPushButton::clicked, this, &MapperProtocolSettingsWidget::validate);

  QGridLayout* gLayout = new QGridLayout;

  gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  gLayout->addWidget(m_name, 0, 1, 1, 1);
  gLayout->addWidget(m_splitter, 3, 0, 1, 2);
  gLayout->addWidget(validateBtn, 4, 0, 1, 2);

  setLayout(gLayout);

  setDefaults();
}

void MapperProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_codeEdit);

  m_name->setText("newDevice");
  m_codeEdit->setPlainText("");
}

void MapperProtocolSettingsWidget::validate()
{
  m_errorPane->clear();

  auto code = m_codeEdit->toPlainText().toUtf8();
  if(code.isEmpty())
    return;

  auto engine = new QQmlEngine{this};
  auto comp = new QQmlComponent{engine};

  connect(
      comp, &QQmlComponent::statusChanged, this,
      [this, comp, engine](QQmlComponent::Status status) {
    switch(status)
    {
      case QQmlComponent::Status::Ready:
        m_errorPane->setPlainText(tr("QML code is valid."));
        break;
      case QQmlComponent::Status::Error: {
        auto errors = comp->errorString();
        if(errors.startsWith(':'))
          errors = QStringLiteral("L") + errors.mid(1);
        errors.replace(QStringLiteral("\n:"), QStringLiteral("\nL"));
        qDebug() << "Mapper:" << errors;
        m_errorPane->setPlainText(errors);
      } break;
      default:
        break;
    }
    comp->deleteLater();
    engine->deleteLater();
  });

  comp->setData(code, QUrl{});
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

W_OBJECT_IMPL(Protocols::MapperDevice)
W_OBJECT_IMPL(ossia::net::observable_device_roots)
W_OBJECT_IMPL(ossia::net::mapper_protocol)
#endif
