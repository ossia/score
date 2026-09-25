#include "ReferenceIndex.hpp"

#include <State/Expression.hpp>
#include <State/Relation.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/State/MessageNode.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableReference.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/Column.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(LocalTree::ReferenceIndex)
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/node_functions.hpp>

namespace LocalTree
{
namespace
{
template <typename F>
void forEachAddress(QObject& referrer, F&& f)
{
  if(auto state = qobject_cast<Scenario::StateModel*>(&referrer))
  {
    auto visit = [&](auto& self, const Process::MessageNode& n) -> void {
      if(n.values.userValue)
        f(Process::address(n));
      for(auto& child : n)
        self(self, child);
    };
    visit(visit, state->messages().rootNode());
  }
  else if(auto port = qobject_cast<Process::Port*>(&referrer))
  {
    f(port->address());
  }
  else
  {
    ::State::Expression expr;
    if(auto ev = qobject_cast<Scenario::EventModel*>(&referrer))
      expr = ev->condition();
    else if(auto ts = qobject_cast<Scenario::TimeSyncModel*>(&referrer))
      expr = ts->expression();
    else
      return;

    auto visit = [&](auto& self, const ::State::Expression& e) -> void {
      auto member = [&](const ::State::RelationMember& m) {
        if(auto a = m.target<::State::Address>())
          f(::State::AddressAccessor{*a});
        else if(auto a = m.target<::State::AddressAccessor>())
          f(*a);
      };
      if(auto rel = e.target<::State::Relation>())
      {
        member(rel->lhs);
        member(rel->rhs);
      }
      else if(auto pulse = e.target<::State::Pulse>())
        f(::State::AddressAccessor{pulse->address});
      for(auto& child : e)
        self(self, child);
    };
    visit(visit, expr);
  }
}

}

ReferenceIndex::ReferenceIndex(DocumentPlugin& tree)
    : m_tree{tree}
{
  // Devices publish their nodes one at a time
  m_explorerChanges.setSingleShot(true);
  m_explorerChanges.setInterval(100);
  connect(&m_explorerChanges, &QTimer::timeout, this, [this] {
    const bool all = std::exchange(m_allDevicesChanged, false);
    const bool missing = std::exchange(m_missingDevicesChanged, false);
    auto devices = std::move(m_changedDevices);
    m_changedDevices.clear();
    if(missing && !all)
    {
      ossia::hash_set<QString> present;
      if(auto explorer = m_tree.context().findPlugin<Explorer::DeviceDocumentPlugin>())
        for(auto& dev : explorer->rootNode())
          present.insert(dev.get<Device::DeviceSettings>().name);
      for(auto& [device, referrers] : m_byDevice)
        if(!present.contains(device))
          devices.insert(device);
    }
    for(auto& [device, referrers] : m_byDevice)
      if(all || devices.contains(device))
        for(auto referrer : referrers)
          notify(referrer);
  });
}

ReferenceIndex::~ReferenceIndex() = default;

void ReferenceIndex::add(QObject& referrer)
{
  if(m_entries.find(&referrer) != m_entries.end())
    return;
  m_entries.emplace(&referrer, Entry{});

  auto changed = [this, &referrer] { update(referrer); };
  if(auto state = qobject_cast<Scenario::StateModel*>(&referrer))
    con(*state, &Scenario::StateModel::sig_statesUpdated, this, changed);
  else if(auto port = qobject_cast<Process::Port*>(&referrer))
    con(*port, &Process::Port::addressChanged, this, changed);
  else if(auto ev = qobject_cast<Scenario::EventModel*>(&referrer))
    con(*ev, &Scenario::EventModel::conditionChanged, this, changed);
  else if(auto ts = qobject_cast<Scenario::TimeSyncModel*>(&referrer))
    con(*ts, &Scenario::TimeSyncModel::triggerChanged, this, changed);
  else
  {
    m_entries.erase(&referrer);
    return;
  }
  con(referrer, &QObject::destroyed, this, [this, &referrer] { remove(&referrer); });

  update(referrer);
}

void ReferenceIndex::update(QObject& referrer)
{
  auto it = m_entries.find(&referrer);
  if(it == m_entries.end())
    return;
  auto& entry = it->second;

  const auto& ctx = m_tree.context();
  // An anchor that now resolves to a different object under a different name
  // has lost its target, e.g. a rebuilt port whose id was reused.
  ossia::hash_map<::State::Address, const QObject*, std::hash<::State::Address>> previous;
  ossia::hash_set<::State::Address, std::hash<::State::Address>> wasMisanchored;
  for(auto& r : entry.refs)
  {
    if(r.key)
      previous.emplace(r.address, r.key);
    if(r.misanchored)
      wasMisanchored.insert(r.address);
    unlink(r.key, &referrer);
    if(r.local)
      unlinkAddress(r.address, &referrer);
    else
      unlinkDevice(r.address.device, &referrer);
  }
  entry.refs.clear();
  auto replaced = [&](const ::State::AddressAccessor& acc,
                      const QObject* now) -> const QObject* {
    auto it = previous.find(acc.address);
    if(it != previous.end() && it->second != now && derived(acc, ctx) != acc)
      return it->second;
    return nullptr;
  };

  const auto local = QString::fromStdString(m_tree.device().get_name());
  std::vector<::State::Address> misanchored;
  forEachAddress(referrer, [&](const ::State::AddressAccessor& acc) {
    Ref ref{acc.address, nullptr, nullptr, bool(acc.address.anchor), acc.address.device == local};
    if(ref.anchored && ref.local)
      ref.target = acc.address.anchor->resolve(ctx);
    if(ref.target && namesOther(acc.address, ref.target))
    {
      ref.target = nullptr;
      ref.misanchored = true;
      // Recover only once, or a rewrite that cannot change the referrer loops
      if(!wasMisanchored.contains(acc.address))
        misanchored.push_back(acc.address);
    }
    else if(ref.target)
    {
      if(auto lost = replaced(acc, ref.target))
      {
        // Keyed on the lost target so later updates do not rebind to it
        ref.target = nullptr;
        ref.key = lost;
      }
    }
    if(ref.target)
    {
      ref.key = ref.target.data();
      auto& by = m_byTarget[ref.key];
      if(!ossia::contains(by, &referrer))
        by.push_back(&referrer);
      // Before its address can be reused
      if(m_watched.insert(ref.key).second)
        connect(ref.target, &QObject::destroyed, this, [this, key = ref.key] {
          unpublished(key);
          m_byTarget.erase(key);
          m_watched.erase(key);
        });
    }
    if(!ref.address.device.isEmpty())
    {
      auto& by = ref.local ? m_byAddress[ref.address] : m_byDevice[ref.address.device];
      if(!ossia::contains(by, &referrer))
        by.push_back(&referrer);
    }
    entry.refs.push_back(std::move(ref));
  });
  notify(&referrer);

  for(auto& a : misanchored)
    QMetaObject::invokeMethod(this, [this, a] { recover(a); }, Qt::QueuedConnection);
}

bool ReferenceIndex::namesOther(const ::State::Address& a, const QObject* target) const
{
  auto& dev = m_tree.device();
  auto node = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  if(!node || ossia::net::get_zombie(*node))
    return false;
  QString member;
  auto obj = m_tree.objectAt(*node, member);
  return obj && obj != target;
}

void ReferenceIndex::unlinkAddress(const ::State::Address& address, QObject* referrer)
{
  if(auto by = m_byAddress.find(address); by != m_byAddress.end())
  {
    ossia::remove_erase(by->second, referrer);
    if(by->second.empty())
      m_byAddress.erase(by);
  }
}

void ReferenceIndex::unlinkDevice(const QString& device, QObject* referrer)
{
  if(auto by = m_byDevice.find(device); by != m_byDevice.end())
  {
    ossia::remove_erase(by->second, referrer);
    if(by->second.empty())
      m_byDevice.erase(by);
  }
}

void ReferenceIndex::explorerChanged(const QString& device)
{
  if(!m_allDevicesChanged)
    m_changedDevices.insert(device);
  if(!m_explorerChanges.isActive())
    m_explorerChanges.start();
}

void ReferenceIndex::explorerDevicesChanged()
{
  m_missingDevicesChanged = true;
  if(!m_explorerChanges.isActive())
    m_explorerChanges.start();
}

void ReferenceIndex::unpublished(const QObject* target)
{
  if(auto it = m_byTarget.find(target); it != m_byTarget.end())
    for(auto referrer : it->second)
      notify(referrer);
}

void ReferenceIndex::notify(QObject* referrer)
{
  if(!referrer)
  {
    m_pendingAll = true;
    m_pending.clear();
  }
  else if(!m_pendingAll)
  {
    m_pending.insert(referrer);
  }
  schedule();
}

void ReferenceIndex::schedule()
{
  if(m_scheduled)
    return;
  m_scheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_scheduled = false;
    const bool all = std::exchange(m_pendingAll, false);
    auto pending = std::move(m_pending);
    m_pending.clear();
    if(all)
      m_tree.referencesChanged(nullptr);
    else
      for(auto referrer : pending)
        m_tree.referencesChanged(referrer);
    changed();
  });
}

void ReferenceIndex::unlink(const QObject* target, QObject* referrer)
{
  if(!target)
    return;
  if(auto by = m_byTarget.find(target); by != m_byTarget.end())
  {
    ossia::remove_erase(by->second, referrer);
    if(by->second.empty())
      m_byTarget.erase(by);
  }
}

void ReferenceIndex::remove(QObject* referrer)
{
  auto it = m_entries.find(referrer);
  if(it == m_entries.end())
    return;
  for(auto& r : it->second.refs)
  {
    unlink(r.key, referrer);
    if(r.local)
      unlinkAddress(r.address, referrer);
    else
      unlinkDevice(r.address.device, referrer);
  }
  m_entries.erase(it);
  m_pending.erase(referrer);
  // The other referrers are unaffected
  schedule();
}

std::vector<QObject*> ReferenceIndex::referrers(const QObject& target) const
{
  if(auto it = m_byTarget.find(&target); it != m_byTarget.end())
    return it->second;
  return {};
}

std::vector<QObject*> ReferenceIndex::targets(const QObject& referrer) const
{
  std::vector<QObject*> res;
  if(auto it = m_entries.find(const_cast<QObject*>(&referrer)); it != m_entries.end())
    for(auto& r : it->second.refs)
      if(r.target && !ossia::contains(res, r.target.data()))
        res.push_back(r.target);
  return res;
}

std::vector<ReferenceIndex::Broken> ReferenceIndex::broken() const
{
  const auto& ctx = m_tree.context();
  std::vector<Broken> res;
  for(auto& [referrer, entry] : m_entries)
    for(auto& r : entry.refs)
    {
      if(r.local)
      {
        if(r.dangling() || addressStatus(r.address, ctx) != AddressStatus::Found)
          res.push_back({r.address, referrer, AddressStatus::Unpublished});
      }
      else if(auto s = addressStatus(r.address, ctx); s != AddressStatus::Found)
        res.push_back({r.address, referrer, s});
    }
  return res;
}

void ReferenceIndex::watchExplorer()
{
  auto devices = m_tree.context().findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devices)
    return;
  auto& explorer = devices->explorer();
  connect(
      &explorer, &QAbstractItemModel::rowsInserted, this,
      [this, &explorer](const QModelIndex& parent, int first, int last) {
    if(parent.isValid())
    {
      explorerChanged(Device::deviceName(explorer.nodeFromModelIndex(parent)));
      return;
    }
    for(int row = first; row <= last; row++)
      explorerChanged(
          Device::deviceName(explorer.nodeFromModelIndex(explorer.index(row, 0, parent))));
  });
  connect(
      &explorer, &QAbstractItemModel::rowsRemoved, this,
      [this, &explorer](const QModelIndex& parent) {
    if(parent.isValid())
      explorerChanged(Device::deviceName(explorer.nodeFromModelIndex(parent)));
    else
      explorerDevicesChanged();
  });
  connect(&explorer, &QAbstractItemModel::modelReset, this, [this] {
    m_allDevicesChanged = true;
    explorerDevicesChanged();
  });
  // Renames only: value updates come at the device's message rate
  connect(
      &explorer, &QAbstractItemModel::dataChanged, this,
      [this, &explorer](const QModelIndex& topLeft) {
    if(topLeft.column() != (int)Explorer::Column::Name)
      return;
    auto& node = explorer.nodeFromModelIndex(topLeft);
    explorerChanged(Device::deviceName(node));
    if(node.is<Device::DeviceSettings>())
      explorerDevicesChanged();
  });
}

void ReferenceIndex::rewrite(QObject& referrer, const Mapping& map)
{
  const auto& ctx = m_tree.context();
  if(auto state = qobject_cast<Scenario::StateModel*>(&referrer))
  {
    auto tree = state->messages().rootNode();
    std::vector<std::pair<::State::Message, ::State::AddressAccessor>> moves;
    auto visit = [&](auto& self, const Process::MessageNode& n) -> void {
      if(n.values.userValue)
      {
        auto acc = Process::address(n);
        if(auto next = map(acc))
          moves.push_back({::State::Message{*next, *n.values.userValue}, acc});
      }
      for(auto& child : n)
        self(self, child);
    };
    visit(visit, tree);
    if(moves.empty())
      return;
    for(auto& [message, old] : moves)
    {
      Scenario::updateTreeWithRemovedUserMessage(tree, old);
      Scenario::updateTreeWithMessageList(tree, {message});
    }
    state->messages() = std::move(tree);
  }
  else if(auto port = qobject_cast<Process::Port*>(&referrer))
  {
    if(auto next = map(port->address()))
      port->setAddress(*next);
  }
  else if(auto ev = qobject_cast<Scenario::EventModel*>(&referrer))
  {
    auto e = ev->condition();
    bool changed = false;
    mapAddresses(e, map, changed);
    if(changed)
      ev->setCondition(e);
  }
  else if(auto ts = qobject_cast<Scenario::TimeSyncModel*>(&referrer))
  {
    auto e = ts->expression();
    bool changed = false;
    mapAddresses(e, map, changed);
    if(changed)
      ts->setExpression(e);
  }
  (void)ctx;
}

void ReferenceIndex::refresh(const QObject& target)
{
  const auto& ctx = m_tree.context();
  for(auto referrer : referrers(target))
  {
    rewrite(*referrer, [&](const ::State::AddressAccessor& acc)
                           -> std::optional<::State::AddressAccessor> {
      if(!acc.address.anchor || acc.address.anchor->resolve(ctx) != &target
         || namesOther(acc.address, &target))
        return std::nullopt;
      auto next = derived(acc, ctx);
      if(next == acc)
        return std::nullopt;
      return next;
    });
    notify(referrer);
  }
}

void ReferenceIndex::recover(const ::State::Address& published)
{
  const auto& ctx = m_tree.context();
  std::vector<QObject*> left;
  auto by = m_byAddress.find(published);
  if(by == m_byAddress.end())
    return;
  for(auto referrer : by->second)
  {
    auto entry = m_entries.find(referrer);
    if(entry == m_entries.end())
      continue;
    if(ossia::any_of(entry->second.refs, [&](auto& r) {
         return r.address == published
                && (r.dangling() || (r.local && namesOther(r.address, r.target)));
       }))
      left.push_back(referrer);
  }

  for(auto referrer : left)
  {
    rewrite(*referrer, [&](const ::State::AddressAccessor& acc)
                           -> std::optional<::State::AddressAccessor> {
      if(acc.address != published)
        return std::nullopt;
      if(acc.address.anchor)
        if(auto obj = acc.address.anchor->resolve(ctx); obj && !namesOther(acc.address, obj))
          return std::nullopt;
      auto next = acc;
      next.address.anchor.reset();
      if(!anchor(next, ctx))
        return std::nullopt;
      return next;
    });
    // The anchor may resolve again if the new object took the old path
    update(*referrer);
  }
}
}
