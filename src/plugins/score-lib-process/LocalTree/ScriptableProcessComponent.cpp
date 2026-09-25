#include "ScriptableProcessComponent.hpp"

#include <LocalTree/ScriptableReference.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessState.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/tools/Bind.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/parameter.hpp>

#include <ossia-qt/invoke.hpp>

#include <QApplication>
#include <QPointer>
#include <QTimer>

#include <atomic>

namespace LocalTree
{
::State::Address addressOfNode(const ossia::net::node_base& n)
{
  ::State::Address a;
  a.device = QString::fromStdString(n.get_device().get_name());
  a.path = QString::fromStdString(n.osc_address()).split('/', Qt::SkipEmptyParts);
  return a;
}

void giveName(
    GivenName& given, score::ModelMetadata& metadata, const QString& own,
    const QString& name)
{
  given.name = name;
  given.restore = [m = QPointer{&metadata}, own, name] {
    if(m && m->getName() == name)
      m->setName(own);
  };
  given.active = true;
  metadata.setName(name);
}

void giveName(GivenName& given, Process::Port& port, const QString& name)
{
  given.restore = [p = QPointer{&port}, own = port.hasOwnExposed() ? port.exposed() : QString{},
                   name] {
    if(!p || p->exposed() != name)
      return;
    if(own.isEmpty())
      p->resetExposed();
    else
      p->setExposed(own);
  };
  given.name = name;
  given.active = true;
  port.setExposed(name);
}

struct ScriptableProcessBase::PortEntry
{
  GivenName given;
  QPointer<Process::Port> port;
  //! Key in m_byPort; still usable for erasing after the port is destroyed
  const Process::Port* key{};
  ossia::net::node_base* node{};
  std::unique_ptr<ParameterBinding> value;
  //! Disconnected here: the port can outlive the entry
  std::vector<QMetaObject::Connection> connections;
  ~PortEntry()
  {
    for(auto& c : connections)
      QObject::disconnect(c);
  }
};

struct ScriptableProcessBase::Kept
{
  ossia::net::node_base* node{};
  std::optional<ossia::net::parameter_base::callback_index> callback;
  //! Set from whatever thread writes the parameter
  std::shared_ptr<std::atomic_bool> written = std::make_shared<std::atomic_bool>(false);

  void release()
  {
    if(auto p = node->get_parameter(); p && callback)
      p->remove_callback(*callback);
    callback.reset();
  }
};

// Same type and, for lists, elements of the type of the current ones
static bool sameShape(const ossia::value& current, const ossia::value& v)
{
  if(current.get_type() != v.get_type())
    return false;
  auto cur = current.target<std::vector<ossia::value>>();
  auto next = v.target<std::vector<ossia::value>>();
  if(!cur || cur->empty() || !next)
    return true;
  const auto t = cur->front().get_type();
  return ossia::all_of(*next, [t](const ossia::value& e) { return e.get_type() == t; });
}

namespace
{
//! Two-way sync between a port value and its parameter, without echo.
template <typename Property>
struct ValueBinding final : ParameterBinding
{
  using model_t = typename Property::model_type;
  static constexpr bool editable = std::is_same_v<model_t, Process::ControlInlet>;
  struct guard
  {
    std::atomic_bool from_model{};
    bool from_param{};
  };
  std::shared_ptr<guard> busy = std::make_shared<guard>();
  QPointer<model_t> model;
  std::function<ControlWrites(Recall)> writes;
  ScriptableTreeBase* tree{};
  std::function<void(Process::ControlInlet&, const ossia::value&)> recorded;
  std::function<void(Process::ControlInlet&, const ossia::value&)> played;

  //! Writes while stopped are grouped into one undoable edit, committed after a pause
  std::optional<ossia::value> burstStart;
  //! Holds the document modified while a burst is pending
  QPointer<score::CommandStack> burstStack;
  //! For a control that changes ports: the burst's command, pushed when it ends
  std::unique_ptr<Process::ChangePortsCommand> portsCommand;
  QTimer burstEnd;
  QMetaObject::Connection stopped;

  //! Lock-free hand-off of the latest written value to the GUI thread
  struct slot
  {
    struct write
    {
      ossia::value value;
      Recall recall{};
      uint32_t run{};
    };
    std::atomic<write*> latest{};
    std::atomic<write*> spare{};
    std::atomic_bool posted{};

    void put(const ossia::value& v, Recall recall, uint32_t run)
    {
      auto w = spare.exchange(nullptr);
      if(!w)
        w = new write;
      w->value = v;
      w->recall = recall;
      w->run = run;
      recycle(latest.exchange(w));
    }
    std::unique_ptr<write> take() { return std::unique_ptr<write>{latest.exchange(nullptr)}; }
    void recycle(write* w) { delete spare.exchange(w); }
    ~slot()
    {
      delete latest.load();
      delete spare.load();
    }
  };
  std::shared_ptr<slot> pending = std::make_shared<slot>();

  ValueBinding(
      ossia::net::parameter_base& p, model_t& m, const ScriptableRoots& roots,
      QObject* context)
      : ParameterBinding{p}
      , model{&m}
      , writes{roots.writes}
      , recorded{roots.recorded}
      , played{roots.played}
      , tree{roots.tree}
  {
    param.set_value((m.*Property::get)());

    connection = QObject::connect(
        &m, Property::notify, context,
        [this, busy = busy, alive = alive] {
      if(!*alive || !model || busy->from_param)
        return;
      // Changed elsewhere, e.g. undone: drop the pending burst
      if constexpr(editable)
        if(burstStart && !portsCommand)
        {
          burstStart.reset();
          burstEnd.stop();
          if(auto stack = std::exchange(burstStack, nullptr))
            stack->endPendingEdit();
        }
      pushModel();
    });

    if(roots.tree)
      stopped = QObject::connect(
          roots.tree, &ScriptableTreeBase::executionStopped, context,
          [this, alive = alive] {
        if(*alive && model)
          pushModel();
      });

    burstEnd.setSingleShot(true);
    burstEnd.setInterval(250);
    QObject::connect(&burstEnd, &QTimer::timeout, [this] { commitBurst(); });

    callback = param.add_callback(
        [this, busy = busy, alive = alive, pending = pending,
         tree = tree](const ossia::value& v) {
      if(busy->from_model)
        return;
      pending->put(
          v, tree ? tree->recall() : Recall::None, tree ? tree->runsEnded() : 0);
      if(pending->posted.exchange(true))
        return;
      ossia::qt::run_async(qApp, [this, alive, pending, tree] {
        pending->posted = false;
        auto w = pending->take();
        if(!w)
          return;
        if(*alive && model)
          receive(w->value, tree ? tree->recallAt(w->recall, w->run) : w->recall);
        pending->recycle(w.release());
      });
    });
  }

  ~ValueBinding()
  {
    QObject::disconnect(stopped);
    // A pending edit is committed once the unpublishing command has returned
    if constexpr(editable)
      if(burstStart && burstStack)
        QTimer::singleShot(
            0, burstStack.data(),
            [stack = burstStack, m = model, before = *burstStart,
             pending = std::make_shared<std::unique_ptr<Process::ChangePortsCommand>>(
                 std::move(portsCommand))] {
          // Otherwise deleted with the lambda, even if the stack is gone first
          if(auto& cmd = *pending)
          {
            if(m)
              score::IDocument::documentContext(*m).commandStack.push(cmd.release());
          }
          else if(m)
          {
            submitEdit(*m, before);
          }
          stack->endPendingEdit();
        });
  }

  //! Submits the change from before to the current value as one undoable edit
  static void submitEdit(model_t& m, const ossia::value& before)
  {
    const auto after = (m.*Property::get)();
    if(before == after)
      return;
    // Restore the previous value so the command's undo goes back to it
    (m.*Property::set)(before);
    CommandDispatcher<>{score::IDocument::documentContext(m).commandStack}
        .submit<Process::SetValue>(m, after);
  }

  void pushModel()
  {
    busy->from_model = true;
    const auto& v = ((*model).*Property::get)();
    if(param.value() != v)
      param.push_value(v);
    busy->from_model = false;
  }

  void setModel(const ossia::value& v)
  {
    busy->from_param = true;
    ((*model).*Property::set)(v);
    busy->from_param = false;
  }

  void write(const ossia::value& v) override
  {
    if(model)
      receive(v, tree ? tree->recall() : Recall::None);
  }

  void receive(const ossia::value& v, Recall recall)
  {
    const auto mode = writes ? writes(recall) : ControlWrites::Edit;
    if(mode == ControlWrites::Drop)
    {
      pushModel();
      return;
    }
    if constexpr(editable)
    {
      // Port changes cannot be execution-only: always an edit
      if(model->changesPorts)
      {
        writePorts(v);
        return;
      }
    }
    if(mode == ControlWrites::Execution)
    {
      // A cable or an address of its own drives the execution instead
      if(model->cables().empty() && !model->address().isSet())
      {
        model->setExecutionValue(v);
        if constexpr(editable)
          if(played)
            played(*model, v);
      }
      return;
    }
    if constexpr(editable)
    {
      if(mode == ControlWrites::Record)
      {
        if(recorded)
          recorded(*model, model->value());
      }
      else
      {
        if(!burstStart)
          beginBurst();
        burstEnd.start();
      }
    }
    setModel(v);
  }

  void writePorts(const ossia::value& v)
  {
    if constexpr(editable)
    {
      // e.g. a float from an automation into a list of rows
      const auto& current = model->value();
      if(current.valid() && !sameShape(current, v))
      {
        pushModel();
        return;
      }
      if(!portsCommand && current == v)
        return;

      const auto& ctx = score::IDocument::documentContext(*model);
      busy->from_param = true;
      if(!portsCommand)
      {
        portsCommand.reset(Process::makeChangePortsCommand(*model, v, ctx));
        if(!portsCommand)
        {
          busy->from_param = false;
          // No factory: the value alone, as one edit
          if(!burstStart)
            beginBurst();
          setModel(v);
          burstEnd.start();
          return;
        }
        beginBurst();
      }
      else
      {
        portsCommand->setNewValue(v);
      }
      portsCommand->redo(ctx);
      busy->from_param = false;
      burstEnd.start();
    }
  }

  //! Marks the document modified until the burst is committed
  void beginBurst()
  {
    if constexpr(editable)
    {
      burstStart = model->value();
      if(auto doc = score::IDocument::try_documentFromObject(*model))
      {
        burstStack = &doc->commandStack();
        burstStack->beginPendingEdit();
      }
    }
  }

  void commitBurst()
  {
    if constexpr(editable)
    {
      if(!burstStart)
        return;
      const auto before = *std::exchange(burstStart, std::nullopt);
      if(auto cmd = portsCommand.release())
      {
        if(model)
          score::IDocument::documentContext(*model).commandStack.push(cmd);
        else
          delete cmd;
      }
      else if(model)
      {
        busy->from_param = true;
        submitEdit(*model, before);
        busy->from_param = false;
      }
      if(auto stack = std::exchange(burstStack, nullptr))
        stack->endPendingEdit();
    }
  }
};

bool isControl(const Process::Port* p) noexcept
{
  return qobject_cast<const Process::ControlInlet*>(p)
         || qobject_cast<const Process::ControlOutlet*>(p);
}

const ossia::value& portValue(const Process::Port& p) noexcept
{
  if(auto in = qobject_cast<const Process::ControlInlet*>(&p))
    return in->value();
  return static_cast<const Process::ControlOutlet&>(p).value();
}
}

ScriptableProcessBase::ScriptableProcessBase(
    ScriptableRoots roots, Process::ProcessModel& proc,
    const score::DocumentContext& ctx, QObject* parent)
    : Process::GenericProcessComponent<const score::DocumentContext>{
        proc, ctx, QStringLiteral("ScriptableProcessComponent"), parent}
    , m_roots{roots}
{
  auto refresh = [this] { this->refresh(); };
  con(proc, &Process::ProcessModel::scriptableChanged, this, refresh);
  con(proc, &Process::ProcessModel::inletsChanged, this, refresh);
  con(proc, &Process::ProcessModel::outletsChanged, this, refresh);
  con(proc, &Process::ProcessModel::controlAdded, this, refresh);
  con(proc, &Process::ProcessModel::controlRemoved, this, refresh);
  con(proc, &Process::ProcessModel::controlOutletAdded, this, refresh);
  con(proc, &Process::ProcessModel::controlOutletRemoved, this, refresh);
  con(proc.metadata(), &score::ModelMetadata::NameChanged, this,
      [this] { renameNode(); });

  this->refresh();
}

ScriptableProcessBase::~ScriptableProcessBase()
{
  if(m_node)
    removeNode(false);
}

ossia::net::node_base* ScriptableProcessBase::node(const Process::Port& port) const noexcept
{
  if(auto it = m_byPort.find(&port); it != m_byPort.end() && it->second->port)
    return it->second->node;
  return nullptr;
}

ossia::net::node_base* ScriptableProcessBase::stateNode() const noexcept
{
  return m_state ? &m_state->param.get_node() : nullptr;
}

void ScriptableProcessBase::refresh()
{
  auto& proc = process();

  // Ports removed from the process, including already destroyed ones
  ossia::hash_set<const Process::Port*> ports;
  ports.reserve(proc.inlets().size() + proc.outlets().size());
  ports.insert(proc.inlets().begin(), proc.inlets().end());
  ports.insert(proc.outlets().begin(), proc.outlets().end());
  auto current = [&](const PortEntry& e) { return e.port && ports.contains(e.key); };
  const bool dynamic = bool(proc.flags() & Process::ProcessFlags::DynamicPorts);
  for(auto& e : m_ports)
  {
    if(current(*e))
      continue;
    if(dynamic && e->node)
    {
      e->value.reset();
      keep(*e->node, true);
      e->node = nullptr;
    }
    else
    {
      unpublish(*e, false);
    }
  }
  ossia::remove_erase_if(m_ports, [&](auto& e) {
    if(current(*e))
      return false;
    m_byPort.erase(e->key);
    return true;
  });

  auto track = [this](Process::Port* p) {
    if(!isControl(p))
      return;
    if(m_byPort.contains(p))
      return;

    auto& e = *m_ports.emplace_back(std::make_unique<PortEntry>());
    e.port = p;
    e.key = p;
    m_byPort[p] = &e;
    auto& c = e.connections;
    c.push_back(con(*p, &Process::Port::scriptableChanged, this, [this] { this->refresh(); }));
    c.push_back(con(*p, &Process::Port::exposedChanged, this, [this, &e, p](const QString& name) {
      if(!e.node)
        return;
      // Also emitted when the namespace gave the name
      if(e.node->get_name() == name.toStdString())
        return;
      dropKept(name);
      e.given.follow(name);
      renameScriptableNode(
          *e.node, name, [&e, p](const QString& n) { giveName(e.given, *p, n); });
    }));
    if(auto in = qobject_cast<Process::ControlInlet*>(p))
    {
      c.push_back(con(*in, &Process::ControlInlet::domainChanged, this, [&e](const State::Domain& d) {
        if(e.node)
          if(auto param = e.node->get_parameter())
            param->set_domain(d);
      }));
      c.push_back(con(*in, &Process::ControlInlet::valueChanged, this, [&e](const ossia::value& v) {
        if(e.node && v.valid())
          if(auto param = e.node->get_parameter(); param->get_value_type() != v.get_type())
            param->set_value_type(v.get_type());
      }));
    }
  };
  for(auto p : proc.inlets())
  {
    announceReferrer(m_roots, *p);
    track(p);
  }
  for(auto p : proc.outlets())
  {
    announceReferrer(m_roots, *p);
    track(p);
  }

  const bool wanted
      = proc.scriptable()
        || ossia::any_of(m_ports, [](auto& e) { return e->port->scriptable(); });
  if(!wanted)
  {
    if(m_node)
      removeNode(true);
    return;
  }

  if(!m_node)
  {
    const auto own = proc.metadata().getName();
    m_given.follow(own);
    m_node = createScriptableNode(
        m_roots, m_roots.controls, own,
        [this, &proc, own](const QString& n) { giveName(m_given, proc.metadata(), own, n); });
    if(!m_node)
      return;
    tagNode(m_roots, *m_node, proc);
  }

  // Before the ports, so that no port takes the "state" name
  syncState();
  for(auto& e : m_ports)
  {
    if(wantsPublished(*e->port) && !e->node)
      publish(*e);
    else if(!wantsPublished(*e->port) && e->node)
      unpublish(*e, true);
  }
}

void ScriptableProcessBase::renameNode()
{
  if(!m_node)
    return;
  auto& proc = process();
  const auto own = proc.metadata().getName();
  // Also emitted when the namespace gave the name
  if(m_node->get_name() == own.toStdString())
    return;
  m_given.follow(own);
  renameScriptableNode(*m_node, own, [this, &proc, own](const QString& n) {
    giveName(m_given, proc.metadata(), own, n);
  });
}

void ScriptableProcessBase::removeNode(bool restoreNames)
{
  for(auto& e : m_ports)
    unpublish(*e, restoreNames);
  for(auto& k : m_kept)
  {
    k->release();
    retireNode(m_roots, *k->node);
  }
  m_kept.clear();
  if(m_state)
  {
    auto& state = m_state->param.get_node();
    m_state.reset();
    retireNode(m_roots, state);
  }
  retireNode(m_roots, *m_node);
  m_node = nullptr;
  if(restoreNames)
    m_given();
  else
    m_given.active = false;
}

void ScriptableProcessBase::publish(PortEntry& e)
{
  auto& port = *e.port;
  std::optional<ossia::value> written;
  auto node = takeKept(port.exposed(), written);
  // Undone or redone, the port gets the value its command holds
  if(system().document.commandStack().isReplaying())
    written.reset();
  if(!node)
  {
    e.given.follow(port.exposed());
    node = createScriptableNode(m_roots, *m_node, port.exposed(), [&e, &port](const QString& n) {
      giveName(e.given, port, n);
    });
  }
  if(!node)
    return;
  // Created by an edit of its process: the value written meanwhile is its initial value
  if(written)
    if(auto in = qobject_cast<Process::ControlInlet*>(&port))
      in->setValue(*std::exchange(written, std::nullopt));

  const auto& v = portValue(port);
  auto param = node->create_parameter(v.valid() ? v.get_type() : ossia::val_type::LIST);
  if(auto in = qobject_cast<Process::ControlInlet*>(&port))
  {
    param->set_access(ossia::access_mode::BI);
    param->set_domain(in->domain());
    e.value = std::make_unique<ValueBinding<Process::ControlInlet::p_value>>(
        *param, *in, m_roots, this);
  }
  else
  {
    auto out = static_cast<Process::ControlOutlet*>(&port);
    param->set_access(ossia::access_mode::GET);
    param->set_domain(out->domain());
    e.value = std::make_unique<ValueBinding<Process::ControlOutlet::p_value>>(
        *param, *out, m_roots, this);
  }
  param->set_unit(port.unit().get());
  if(const auto& d = port.description(); !d.isEmpty())
    ossia::net::set_description(*node, d.toStdString());
  tagNode(m_roots, *node, port);
  e.node = node;
  if(written)
    e.value->write(*written);
}

void ScriptableProcessBase::keep(ossia::net::node_base& node, bool withValue)
{
  if(!m_roots.hide)
  {
    retireNode(m_roots, node);
    return;
  }
  m_roots.hide(node);
  auto& k = *m_kept.emplace_back(std::make_unique<Kept>());
  k.node = &node;
  k.written->store(withValue);
  if(auto p = node.get_parameter())
    k.callback = p->add_callback([w = k.written](const ossia::value&) { w->store(true); });
}

ossia::net::node_base*
ScriptableProcessBase::takeKept(const QString& name, std::optional<ossia::value>& written)
{
  const auto n = name.toStdString();
  auto it = ossia::find_if(m_kept, [&](auto& k) { return k->node->get_name() == n; });
  if(it == m_kept.end())
    return nullptr;
  auto& k = **it;
  k.release();
  auto node = k.node;
  if(auto p = node->get_parameter(); p && k.written->load())
    written = p->value();
  m_kept.erase(it);
  ossia::net::set_zombie(*node, false);
  return node;
}

void ScriptableProcessBase::dropKept(const QString& name)
{
  const auto n = name.toStdString();
  auto it = ossia::find_if(m_kept, [&](auto& k) { return k->node->get_name() == n; });
  if(it == m_kept.end())
    return;
  (*it)->release();
  auto node = (*it)->node;
  m_kept.erase(it);
  retireNode(m_roots, *node);
}

ossia::net::parameter_base*
ScriptableProcessBase::reserve(const std::string& name, const ossia::value& sample)
{
  if(!m_node || !(process().flags() & Process::ProcessFlags::DynamicPorts))
    return nullptr;
  if(auto child = m_node->find_child(name))
    return child->get_parameter();
  auto node = m_node->create_child(name);
  if(!node)
    return nullptr;
  if(node->get_name() != name)
  {
    m_node->remove_child(*node);
    return nullptr;
  }
  auto param
      = node->create_parameter(sample.valid() ? sample.get_type() : ossia::val_type::LIST);
  param->set_access(ossia::access_mode::BI);
  keep(*node, false);
  return param;
}

void ScriptableProcessBase::unpublish(PortEntry& e, bool restoreName)
{
  e.value.reset();
  if(e.node)
    retireNode(m_roots, *e.node);
  e.node = nullptr;
  if(restoreName)
    e.given();
  else
    e.given.active = false;
}

void ScriptableProcessBase::syncState()
{
  const bool wanted = process().scriptable() && Process::recallsState(process());
  if(wanted && !m_state)
  {
    auto node = createScriptableNode(
        m_roots, *m_node, QStringLiteral("state"), [](const QString&) {});
    if(!node)
      return;
    tagNode(m_roots, *node, process(), QStringLiteral("state"));
    auto param = node->create_parameter(ossia::val_type::STRING);
    param->set_access(ossia::access_mode::SET);
    m_state = std::make_unique<ParameterBinding>(*param);
    // Shared so that a write, possibly on the audio thread, copies no function
    struct Target
    {
      QPointer<Process::ProcessModel> proc;
      std::shared_ptr<bool> alive;
      std::function<ControlWrites(Recall)> writes;
      ScriptableTreeBase* tree{};
    };
    auto target = std::make_shared<const Target>(
        Target{&process(), m_state->alive, m_roots.writes, m_roots.tree});
    m_state->callback = param->add_callback([target](const ossia::value& v) {
      auto str = v.target<std::string>();
      if(!str)
        return;
      auto tree = target->tree;
      const auto recall = tree ? tree->recall() : Recall::None;
      const auto run = tree ? tree->runsEnded() : 0;
      ossia::qt::run_async(qApp, [target, recall, run, json = *str] {
        auto tree = target->tree;
        if(!*target->alive || !target->proc)
          return;
        if(target->writes
           && target->writes(tree ? tree->recallAt(recall, run) : recall)
                  == ControlWrites::Drop)
          return;
        Process::applyStateBeyondControls(*target->proc, QByteArray::fromStdString(json));
      });
    });
  }
  else if(!wanted && m_state)
  {
    auto& node = m_state->param.get_node();
    m_state.reset();
    retireNode(m_roots, node);
  }
}

static const ScriptableProcessBase* scriptableComponent(const Process::ProcessModel& p)
{
  return findComponent<ScriptableProcessBase>(p.components());
}

::State::Address scriptableAddress(const Process::ProcessModel& proc)
{
  if(auto c = scriptableComponent(proc))
    if(auto n = c->node())
      return addressOfNode(*n);
  return {};
}

::State::Address scriptableAddress(const Process::Port& port)
{
  if(auto proc = Process::parentProcess(&port))
    if(auto c = scriptableComponent(*proc))
      if(auto n = c->node(port))
        return addressOfNode(*n);
  return {};
}

bool publishedWithProcess(const Process::Port& port) noexcept
{
  if(!isControl(&port))
    return false;
  auto proc = Process::parentProcess(&port);
  return proc && proc->scriptable();
}

bool wantsPublished(const Process::Port& port) noexcept
{
  return port.scriptable() || publishedWithProcess(port);
}

::State::Address scriptableStateAddress(const Process::ProcessModel& proc)
{
  if(auto c = scriptableComponent(proc))
    if(auto n = c->stateNode())
      return addressOfNode(*n);
  return {};
}

ScriptableProcessFactory::~ScriptableProcessFactory() = default;
ScriptableProcessFactoryList::~ScriptableProcessFactoryList() = default;

ScriptableProcessBase* makeScriptableProcess(
    ScriptableRoots roots, Process::ProcessModel& proc, const score::DocumentContext& ctx,
    QObject* parent)
{
  auto& factories = ctx.app.interfaces<ScriptableProcessFactoryList>();
  for(auto& factory : factories)
    if(factory.matches(proc))
      return factory.make(roots, proc, ctx, parent);
  return new ScriptableProcessComponent{roots, proc, ctx, parent};
}

ScriptableProcessGroup::ScriptableProcessGroup(
    ScriptableRoots roots, Process::ProcessModel& proc,
    score::EntityMap<Process::ProcessModel>& children, const score::DocumentContext& ctx,
    QObject* parent)
    : ScriptableProcessBase{roots, proc, ctx, parent}
{
  for(auto& child : children)
    add(child);
  children.mutable_added.connect<&ScriptableProcessGroup::add>(this);
  children.removing.connect<&ScriptableProcessGroup::remove>(this);
}

ScriptableProcessGroup::~ScriptableProcessGroup()
{
  for(auto& [proc, comp] : m_children)
    proc->components().remove(comp);
}

void ScriptableProcessGroup::add(Process::ProcessModel& proc)
{
  auto comp = makeScriptableProcess(m_roots, proc, system(), this);
  proc.components().add(comp);
  m_children.emplace_back(&proc, comp);
}

void ScriptableProcessGroup::remove(const Process::ProcessModel& proc)
{
  auto it = ossia::find_if(m_children, [&](auto& p) { return p.first == &proc; });
  if(it == m_children.end())
    return;
  it->first->components().remove(it->second);
  m_children.erase(it);
}

namespace
{
void snapshot(
    ScriptableSnapshot& snap, const ossia::net::node_base& node, const QString& key)
{
  // The map may reallocate when children are added: fill the entry first
  ScriptableSnapshot::Entry entry;
  entry.address = addressOfNode(node).toString();
  if(auto p = node.get_parameter())
    entry.kind = p->get_value_type() == ossia::val_type::IMPULSE ? QStringLiteral("impulse")
                                                                  : QStringLiteral("value");
  else
    entry.kind = QStringLiteral("node");

  std::vector<const ossia::net::node_base*> children;
  for(auto child : node.children_copy())
  {
    if(ossia::net::get_zombie(*child))
      continue;
    entry.children.push_back(QString::fromStdString(child->get_name()));
    children.push_back(child);
  }
  snap.entries[key] = std::move(entry);

  for(auto child : children)
    snapshot(snap, *child, key + '/' + QString::fromStdString(child->get_name()));
}
}

std::shared_ptr<const ScriptableSnapshot>
ScriptableSnapshot::build(const ScriptableRoots& roots)
{
  auto snap = std::make_shared<ScriptableSnapshot>();
  for(auto root : {&roots.controls, &roots.triggers, &roots.conditions})
    snapshot(*snap, *root, QString::fromStdString(root->get_name()));
  return snap;
}
}
