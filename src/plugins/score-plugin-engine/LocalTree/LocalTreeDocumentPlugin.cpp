// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalTreeDocumentPlugin.hpp"

#include <State/Relation.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <LocalTree/ScriptableReference.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Settings/ExplorerModel.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <LocalTree/Device/LocalProtocolFactory.hpp>
#include <LocalTree/Device/LocalSpecificSettings.hpp>
#include <LocalTree/IntervalComponent.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <Process/Process.hpp>
#include <score/model/ComponentUtils.hpp>
#include <LocalTree/ScriptableScenarioComponent.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/common/path.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <QRegularExpression>
#include <QTimer>
#include <ossia/network/local/local.hpp>

namespace LocalTree
{
Device::DeviceSettings defaultSettings(const score::DocumentContext& ctx)
{
  Device::DeviceSettings s;
  s.protocol = Protocols::LocalProtocolFactory::static_concreteKey();
  s.name = QStringLiteral("score");
  Protocols::LocalSpecificSettings specif;
  specif.oscPort = Protocols::LocalProtocolFactory::defaultOscPort;
  specif.wsPort = Protocols::LocalProtocolFactory::defaultWsPort;
  s.deviceSpecificSettings = QVariant::fromValue(specif);
  return s;
}
}

LocalTree::DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx, QObject* parent)
    : ScriptableTreeBase{ctx, "LocalTree::DocumentPlugin", parent}
    , m_localDevice{std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::net::multiplex_protocol>(), "score")}
    , m_localDeviceWrapper{*m_localDevice, ctx, defaultSettings(ctx)}
{
  auto& root = m_localDevice->get_root_node();
  m_controls = root.create_child("controls");
  m_triggers = root.create_child("triggers");
  m_conditions = root.create_child("conditions");
  rebuildSnapshot();

  m_localDevice->on_node_created.connect<&DocumentPlugin::onNodeChanged>(this);
  m_localDevice->on_node_removing.connect<&DocumentPlugin::onNodeChanged>(this);
  m_localDevice->on_node_renamed.connect<&DocumentPlugin::onNodeRenamed>(this);
  m_localDevice->on_parameter_created.connect<&DocumentPlugin::onParameterChanged>(this);
  m_localDevice->on_attribute_modified.connect<&DocumentPlugin::onAttributeChanged>(this);
}

std::shared_ptr<const LocalTree::ScriptableSnapshot>
LocalTree::DocumentPlugin::snapshot() const noexcept
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  return m_snapshot;
}

QObject* LocalTree::DocumentPlugin::objectAt(
    const ossia::net::node_base& node, QString& member) const noexcept
{
  auto it = m_byNode.find(&node);
  if(it == m_byNode.end() || !it->second.object)
    return nullptr;
  member = it->second.member;
  return it->second.object;
}

ossia::net::node_base* LocalTree::DocumentPlugin::nodeOf(
    const QObject& object, const QString& member) const noexcept
{
  auto it = m_byObject.find(&object);
  if(it == m_byObject.end())
    return nullptr;
  for(auto& [m, node] : it->second)
    if(m == member)
      return node;
  return nullptr;
}

void LocalTree::DocumentPlugin::tag(
    ossia::net::node_base& node, const QObject& object, const QString& member)
{
  m_byNode[&node] = Tagged{const_cast<QObject*>(&object), &object, member};
  m_byObject[&object].emplace_back(member, &node);

  m_references.recover(addressOfNode(node));
  m_references.refresh(object);
  published(const_cast<QObject*>(&object));
}

void LocalTree::DocumentPlugin::onNodeRenamed(ossia::net::node_base& node, std::string)
{
  onTreeChanged();

  // Refresh the references of the node and of everything published below it
  auto visit = [this](auto& self, ossia::net::node_base& n) -> void {
    if(auto it = m_byNode.find(&n); it != m_byNode.end() && it->second.object)
    {
      m_references.refresh(*it->second.object);
      m_references.recover(addressOfNode(n));
    }
    for(auto child : n.children_copy())
      self(self, *child);
  };
  visit(visit, node);
}

void LocalTree::DocumentPlugin::onTreeChanged()
{
  // Coalesced: the removal signal comes before the node is removed
  if(m_snapshotScheduled)
    return;
  m_snapshotScheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_snapshotScheduled = false;
    rebuildSnapshot();
  });
}

void LocalTree::DocumentPlugin::rebuildSnapshot()
{
  auto snap
      = ScriptableSnapshot::build(ScriptableRoots{*m_controls, *m_triggers, *m_conditions});
  m_snapshot = std::move(snap);
  snapshotChanged();
}

LocalTree::DocumentPlugin::~DocumentPlugin()
{
  m_localDevice->on_node_created.disconnect<&DocumentPlugin::onNodeChanged>(this);
  m_localDevice->on_node_removing.disconnect<&DocumentPlugin::onNodeChanged>(this);
  m_localDevice->on_node_renamed.disconnect<&DocumentPlugin::onNodeRenamed>(this);
  m_localDevice->on_parameter_created.disconnect<&DocumentPlugin::onParameterChanged>(this);
  m_localDevice->on_attribute_modified.disconnect<&DocumentPlugin::onAttributeChanged>(this);

  cleanupScriptable();
  flushRetired();
  cleanup();

  auto docplug = context().findPlugin<Explorer::DeviceDocumentPlugin>();
  if(docplug)
    docplug->list().setLocalDevice(nullptr);
}

void LocalTree::DocumentPlugin::init()
{
  m_localDeviceWrapper.init();
  m_references.watchExplorer();
  createScriptable();
  // Once every namespace node exists; posted so that it runs before a crash backup's replay
  QMetaObject::invokeMethod(this, &DocumentPlugin::anchorDocument, Qt::QueuedConnection);

  auto& set = m_context.app.settings<Explorer::Settings::Model>();
  if(set.getLocalTree())
  {
    create();
  }

  con(
      set, &Explorer::Settings::Model::LocalTreeChanged, this,
      [this](bool b) {
    if(b)
      create();
    else
      cleanup();
      },
      Qt::QueuedConnection);

  auto docplug = context().findPlugin<Explorer::DeviceDocumentPlugin>();
  if(docplug)
    docplug->list().setLocalDevice(&m_localDeviceWrapper);
}

void LocalTree::DocumentPlugin::on_documentClosing()
{
  cleanupScriptable();
  flushRetired();
  cleanup();
  this->m_localDeviceWrapper.setParent(
      nullptr); // otherwise it gets deleted by DeviceDocumentPlugin
}

void LocalTree::DocumentPlugin::create()
{
  if(m_root)
    cleanup();

  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc);
  if(!scenar)
    return;

  auto& cstr = scenar->baseInterval();
  m_root = new Interval(m_localDevice->get_root_node(), cstr, context(), this);
  cstr.components().push_back(m_root);
}

void LocalTree::DocumentPlugin::cleanup()
{
  if(!m_root)
    return;

  m_root->interval().components().remove(m_root);
  m_root = nullptr;
}

void LocalTree::DocumentPlugin::createScriptable()
{
  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc);
  if(!scenar)
    return;

  auto& cstr = scenar->baseInterval();
  ScriptableRoots roots{*m_controls, *m_triggers, *m_conditions};
  roots.retire = [this](ossia::net::node_base& n) { retire(n); };
  roots.hide = [this](ossia::net::node_base& n) {
    untag(n);
    ossia::net::set_zombie(n, true);
  };
  roots.tag = [this](ossia::net::node_base& n, const QObject& o, const QString& m) {
    tag(n, o, m);
  };
  roots.referrer = [this](QObject& o) { m_references.add(o); };
  roots.tree = this;
  roots.revive = [this](ossia::net::node_base& parent, const std::string& name) {
    return revive(parent, name);
  };
  roots.writes = [this](Recall r) { return controlWrites(r); };
  roots.recorded = [this](Process::ControlInlet& p, const ossia::value& before) {
    recorded(p, before);
  };
  roots.played = [this](Process::ControlInlet& p, const ossia::value& v) { played(p, v); };
  m_scriptable = new ScriptableInterval(roots, cstr, context(), this);
  cstr.components().push_back(m_scriptable);
}

ossia::net::parameter_base* LocalTree::DocumentPlugin::reserve(
    const ::State::Address& address, const ossia::value& sample)
{
  if(address.path.size() < 2
     || address.device != QString::fromStdString(m_localDevice->get_name())
     || ossia::traversal::is_pattern(address.toString().toStdString()))
    return nullptr;
  const auto parentPath = address.path.mid(0, address.path.size() - 1);
  auto parent = ossia::net::find_node(
      m_localDevice->get_root_node(), parentPath.join('/').toStdString());
  if(!parent)
    return nullptr;
  QString member;
  auto proc = qobject_cast<Process::ProcessModel*>(objectAt(*parent, member));
  if(!proc)
    return nullptr;
  auto comp = findComponent<ScriptableProcessBase>(proc->components());
  if(!comp)
    return nullptr;
  return comp->reserve(address.path.last().toStdString(), sample);
}

void LocalTree::DocumentPlugin::untag(const ossia::net::node_base& node)
{
  if(auto it = m_byNode.find(&node); it != m_byNode.end())
  {
    m_references.unpublished(it->second.key);
    if(auto obj = m_byObject.find(it->second.key); obj != m_byObject.end())
    {
      ossia::remove_erase_if(obj->second, [&](auto& p) { return p.second == &node; });
      if(obj->second.empty())
        m_byObject.erase(obj);
    }
    m_byNode.erase(it);
  }
  for(auto child : node.children_copy())
    untag(*child);
}

static bool isBelow(const ossia::net::node_base& node, const ossia::net::node_base& ancestor)
{
  for(auto p = node.get_parent(); p; p = p->get_parent())
    if(p == &ancestor)
      return true;
  return false;
}

void LocalTree::DocumentPlugin::retire(ossia::net::node_base& node)
{
  untag(node);

  auto exec = context().findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->baseScenario())
  {
    // Retired nodes below it are removed with it
    ossia::remove_erase_if(m_retired, [&](auto& r) { return isBelow(*r.first, node); });
    if(auto parent = node.get_parent())
      parent->remove_child(node);
    return;
  }

  if(!m_flushConnected)
  {
    connect(exec, &Execution::DocumentPlugin::cleared, this, &DocumentPlugin::flushRetired);
    m_flushConnected = true;
  }

  // Frees its name but stays alive until the execution releases it
  auto name = node.get_name();
  ossia::net::set_zombie(node, true);
  node.set_name(name + " (retired)");
  m_retired.emplace_back(&node, std::move(name));
}

ossia::net::node_base*
LocalTree::DocumentPlugin::revive(ossia::net::node_base& parent, const std::string& name)
{
  auto it = ossia::find_if(m_retired, [&](auto& r) {
    return r.second == name && r.first->get_parent() == &parent;
  });
  if(it == m_retired.end())
    return nullptr;
  auto node = it->first;
  m_retired.erase(it);
  // Renamed while still hidden, so that it reappears under its own name
  node->set_name(name);
  ossia::net::set_zombie(*node, false);
  return node;
}

void LocalTree::DocumentPlugin::connectToStop()
{
  // Connected lazily: the execution plugin is created after this one
  if(m_stopConnected)
    return;
  auto exec = context().findPlugin<Execution::DocumentPlugin>();
  if(!exec)
    return;
  connect(exec, &Execution::DocumentPlugin::cleared, this, [this] {
    m_runsEnded.fetch_add(1, std::memory_order_acq_rel);
    executionStopped();
    // At stop, after the end state has been taken
    if(recall() == Recall::None)
      commitRecorded(Recall::None);
  });
  connect(exec, &Execution::DocumentPlugin::started, this, [this] {
    executionStarted();
    if(std::exchange(m_played, {}).empty())
      return;
    playedValuesChanged();
  });
  m_stopConnected = true;
}

LocalTree::ControlWrites LocalTree::DocumentPlugin::controlWrites(Recall recall)
{
  auto scenario
      = context().app.findGuiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
  const bool recording = scenario && scenario->editionSettings().recordPlayback();

  switch(recall)
  {
    case Recall::Stop:
      return recording ? ControlWrites::Record : ControlWrites::Drop;
    case Recall::State:
      return ControlWrites::Record;
    case Recall::None:
      break;
  }

  auto exec = context().findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->baseScenario())
    return ControlWrites::Edit;
  connectToStop();
  return recording ? ControlWrites::Record : ControlWrites::Execution;
}

void LocalTree::DocumentPlugin::recorded(
    Process::ControlInlet& port, const ossia::value& before)
{
  // Only the first value is kept: it is the one to restore
  if(ossia::none_of(m_recorded, [&](auto& r) { return r.first == &port; }))
    m_recorded.emplace_back(&port, before);
}

void LocalTree::DocumentPlugin::played(
    Process::ControlInlet& port, const ossia::value& value)
{
  auto it = ossia::find_if(m_played, [&](auto& p) { return p.first == &port; });
  if(it != m_played.end())
  {
    it->second = value;
    return;
  }
  m_played.emplace_back(&port, value);
  if(m_played.size() == 1)
    playedValuesChanged();
}

bool LocalTree::DocumentPlugin::hasPlayedValues() const noexcept
{
  return ossia::any_of(m_played, [](auto& p) { return bool(p.first); });
}

void LocalTree::DocumentPlugin::keepPlayedValues()
{
  auto played = std::exchange(m_played, {});
  if(played.empty())
    return;
  RedoMacroCommandDispatcher<Process::KeepPlayedValues> disp{context().commandStack};
  bool any = false;
  for(auto& [port, value] : played)
  {
    if(!port || port->value() == value)
      continue;
    disp.submit(new Process::SetValue{*port, value});
    any = true;
  }
  if(any)
    disp.commit();
  else
    disp.rollback();
  playedValuesChanged();
}

namespace
{
template <typename Macro>
void commitAs(
    const score::DocumentContext& ctx,
    std::vector<std::pair<QPointer<Process::ControlInlet>, ossia::value>> recorded)
{
  RedoMacroCommandDispatcher<Macro> disp{ctx.commandStack};
  bool any = false;
  for(auto& [port, before] : recorded)
  {
    if(!port)
      continue;
    const auto after = port->value();
    if(after == before)
      continue;
    // Restore the previous value so that the command goes from before to after
    port->setValue(before);
    disp.submit(new Process::SetValue{*port, after});
    any = true;
  }
  if(any)
    disp.commit();
  else
    disp.rollback();
}
}

void LocalTree::DocumentPlugin::commitRecorded(Recall recall)
{
  auto recorded = std::move(m_recorded);
  m_recorded.clear();
  if(recall == Recall::State)
    commitAs<Process::RecallState>(context(), std::move(recorded));
  else
    commitAs<Process::RecordPlayback>(context(), std::move(recorded));
}

LocalTree::StateRecall::StateRecall(
    const score::DocumentContext& ctx, const Execution::DocumentPlugin& exec)
{
  if(exec.baseScenario())
    return;
  if((m_tree = ctx.findPlugin<DocumentPlugin>()))
    m_tree->beginRecall(Recall::State);
}

LocalTree::StateRecall::~StateRecall()
{
  if(m_tree)
    m_tree->endRecall(Recall::State);
}

void LocalTree::DocumentPlugin::beginRecall(Recall recall)
{
  connectToStop();
  m_recall.store(recall, std::memory_order_release);
}

void LocalTree::DocumentPlugin::endRecall(Recall recall)
{
  m_recall.store(Recall::None, std::memory_order_release);
  // Queued to run after the values written by the recall reach this thread
  QMetaObject::invokeMethod(
      this, [this, recall] { commitRecorded(recall); }, Qt::QueuedConnection);
}

void LocalTree::DocumentPlugin::flushRetired()
{
  // Only the topmost retired nodes are removed; their descendants go with them
  std::vector<ossia::net::node_base*> retired;
  for(auto& r : m_retired)
    retired.push_back(r.first);
  m_retired.clear();
  std::vector<ossia::net::node_base*> tops;
  for(auto node : retired)
    if(ossia::none_of(retired, [&](auto other) { return isBelow(*node, *other); }))
      tops.push_back(node);
  for(auto node : tops)
    if(auto parent = node->get_parent())
      parent->remove_child(*node);
}

namespace
{
// Resolves a path of the legacy structural tree: ports under
// /<interval>/processes/<process>/<port>/value, intervals of a scenario under
// /<scenario>/intervals/<interval>, syncs under /<scenario>/syncs/<sync>/trigger
QObject* structuralTarget(Scenario::IntervalModel& base, const QStringList& path)
{
  if(path.isEmpty() || path[0] != base.metadata().getName())
    return nullptr;

  Scenario::IntervalModel* itv = &base;
  Process::ProcessModel* proc{};
  qsizetype i = 1;
  while(i + 1 < path.size())
  {
    if(itv && path[i] == QStringLiteral("processes"))
    {
      proc = nullptr;
      for(auto& p : itv->processes)
        if(p.metadata().getName() == path[i + 1])
          proc = &p;
      if(!proc)
        return nullptr;
      itv = nullptr;
      i += 2;
      continue;
    }
    auto scenar = qobject_cast<Scenario::ProcessModel*>(proc);
    if(scenar && path[i] == QStringLiteral("intervals"))
    {
      itv = nullptr;
      for(auto& c : scenar->intervals)
        if(c.metadata().getName() == path[i + 1])
          itv = &c;
      if(!itv)
        return nullptr;
      proc = nullptr;
      i += 2;
      continue;
    }
    if(scenar && path[i] == QStringLiteral("syncs") && i + 2 < path.size()
       && path[i + 2] == QStringLiteral("trigger"))
    {
      for(auto& s : scenar->timeSyncs)
        if(s.metadata().getName() == path[i + 1])
          return &s;
      return nullptr;
    }
    break;
  }

  // <port> or <port>/value, under a process
  if(!proc || i >= path.size())
    return nullptr;
  if(i + 1 < path.size() && !(i + 2 == path.size() && path[i + 1] == QStringLiteral("value")))
    return nullptr;
  for(auto port : proc->inlets())
    if(port->exposed() == path[i])
      return port;
  for(auto port : proc->outlets())
    if(port->exposed() == path[i])
      return port;
  return nullptr;
}
}

std::optional<::State::AddressAccessor>
LocalTree::DocumentPlugin::migrated(const ::State::AddressAccessor& acc) const
{
  // Legacy local device name: "score (<document>)"
  static const QRegularExpression older{QStringLiteral("^score \\(.*\\)$")};
  const auto& a = acc.address;
  auto devices = context().findPlugin<Explorer::DeviceDocumentPlugin>();

  // A device renamed on load because it had the local device's name, unless
  // the address is one the local device publishes
  if(devices)
    for(auto& [before, after] : devices->renamedOnLoad())
      if(a.device == before && !a.anchor
         && !LocalTree::published(a, context()).value_or(false))
      {
        auto next = acc;
        next.address.device = after;
        next.address.anchor.reset();
        return next;
      }

  if(!older.match(a.device).hasMatch())
    return std::nullopt;
  // Unless an explorer device has that name
  if(devices && devices->list().findDevice(a.device))
    return std::nullopt;

  auto next = acc;
  next.address.device = QString::fromStdString(m_localDevice->get_name());
  next.address.anchor.reset();

  // Use the scriptable address of the target, if it is published
  auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
      &m_context.document.model().modelDelegate());
  if(scenar)
  {
    ::State::Address published;
    auto target = structuralTarget(scenar->baseInterval(), a.path);
    if(auto port = qobject_cast<Process::Port*>(target))
      published = LocalTree::scriptableAddress(*port);
    else if(auto sync = qobject_cast<Scenario::TimeSyncModel*>(target))
      published = LocalTree::scriptableAddress(*sync);
    if(published.isSet())
      next.address = published;
  }
  return next;
}

void LocalTree::DocumentPlugin::follow(QObject& referrer)
{
  const auto& ctx = context();
  const auto local = QString::fromStdString(m_localDevice->get_name());
  m_references.rewrite(
      referrer,
      [&](const ::State::AddressAccessor& acc) -> std::optional<::State::AddressAccessor> {
    if(acc.address.device != local)
      return std::nullopt;
    if(acc.address.anchor)
    {
      auto now = LocalTree::derived(acc, ctx);
      if(now.address.isSet() && now != acc)
        return now;
      return std::nullopt;
    }
    auto next = acc;
    if(LocalTree::anchor(next, ctx))
      return next;
    return std::nullopt;
  });
}

void LocalTree::DocumentPlugin::anchorDocument()
{
  const auto& ctx = context();
  auto& root = m_context.document.model();

  std::vector<Scenario::StateModel*> states;
  std::vector<Process::Port*> ports;
  std::vector<Scenario::EventModel*> events;
  std::vector<Scenario::TimeSyncModel*> syncs;
  for(auto obj : root.findChildren<QObject*>())
  {
    if(auto state = qobject_cast<Scenario::StateModel*>(obj))
      states.push_back(state);
    else if(auto port = qobject_cast<Process::Port*>(obj))
      ports.push_back(port);
    else if(auto event = qobject_cast<Scenario::EventModel*>(obj))
      events.push_back(event);
    else if(auto sync = qobject_cast<Scenario::TimeSyncModel*>(obj))
      syncs.push_back(sync);
  }

  // Migrate legacy structural addresses
  auto migrate = [this](const ::State::AddressAccessor& a) { return migrated(a); };
  for(auto state : states)
    m_references.rewrite(*state, migrate);
  for(auto port : ports)
    m_references.rewrite(*port, migrate);
  for(auto event : events)
    m_references.rewrite(*event, migrate);
  for(auto sync : syncs)
    m_references.rewrite(*sync, migrate);

  for(auto state : states)
  {
    auto tree = state->messages().rootNode();
    if(LocalTree::settle(tree, ctx))
      state->messages() = std::move(tree);
    m_references.add(*state);
  }
  for(auto port : ports)
  {
    if(auto a = port->address(); a.isSet())
    {
      LocalTree::settle(a, ctx);
      port->setAddress(a);
    }
    m_references.add(*port);
  }
  for(auto event : events)
  {
    auto e = event->condition();
    LocalTree::settle(e, ctx);
    event->setCondition(e);
    m_references.add(*event);
  }
  for(auto sync : syncs)
  {
    auto e = sync->expression();
    LocalTree::settle(e, ctx);
    sync->setExpression(e);
    m_references.add(*sync);
  }
}

void LocalTree::DocumentPlugin::cleanupScriptable()
{
  if(!m_scriptable)
    return;

  m_scriptable->interval().components().remove(m_scriptable);
  m_scriptable = nullptr;
}
