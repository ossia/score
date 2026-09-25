#include "ScriptableReference.hpp"

#include <State/Relation.hpp>

#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Widgets/DeviceModelProvider.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>

#include <score/application/ApplicationContext.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <wobjectimpl.h>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/common/path.hpp>

#include <QObject>

W_OBJECT_IMPL(LocalTree::ScriptableTreeBase)

namespace LocalTree
{
ScriptableTreeBase::~ScriptableTreeBase() = default;

void ScriptableTreeBase::setEmphasized(std::vector<const QObject*> objects)
{
  if(objects.size() == m_emphasized.size()
     && std::equal(objects.begin(), objects.end(), m_emphasized.begin(),
                   [](auto a, auto& b) { return a == b.data(); }))
    return;
  m_emphasized.assign(objects.begin(), objects.end());
  emphasisChanged();
}

bool ScriptableTreeBase::emphasized(const QObject& object) const noexcept
{
  return ossia::any_of(m_emphasized, [&](auto& p) { return p.data() == &object; });
}

namespace
{
ScriptableTreeBase* tree(const score::DocumentContext& ctx) noexcept
{
  return ctx.findPlugin<ScriptableTreeBase>();
}
}

bool anchor(::State::Address& a, const score::DocumentContext& ctx)
{
  if(a.anchor)
    return true;
  auto t = tree(ctx);
  if(!t)
    return false;

  auto& dev = t->device();
  if(a.device.toStdString() != dev.get_name())
    return false;
  auto node = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  if(!node)
    return false;

  QString member;
  auto obj = t->objectAt(*node, member);
  if(!obj)
    return false;

  a.anchor = std::make_shared<const ::State::Anchor>(
      ::State::Anchor{score::IDocument::unsafe_path(*obj), std::move(member)});
  return true;
}

bool anchor(::State::AddressAccessor& a, const score::DocumentContext& ctx)
{
  return anchor(a.address, ctx);
}

void anchor(::State::Expression& e, const score::DocumentContext& ctx)
{
  auto member = [&](::State::RelationMember& m) {
    if(auto a = m.target<::State::Address>())
      anchor(*a, ctx);
    else if(auto a = m.target<::State::AddressAccessor>())
      anchor(*a, ctx);
  };
  if(auto rel = e.target<::State::Relation>())
  {
    member(rel->lhs);
    member(rel->rhs);
  }
  else if(auto pulse = e.target<::State::Pulse>())
  {
    anchor(pulse->address, ctx);
  }
  for(auto& child : e)
    anchor(child, ctx);
}

void settle(::State::Address& a, const score::DocumentContext& ctx)
{
  if(a.anchor)
  {
    if(auto d = derive(*a.anchor, ctx); d.isSet())
    {
      if(d != a)
        a.anchor.reset();
    }
    else
    {
      // The anchor designates nothing published here: an object publishing
      // the address takes it over
      auto byName = a;
      byName.anchor.reset();
      if(anchor(byName, ctx))
        a = std::move(byName);
    }
  }
  anchor(a, ctx);
}

void settle(::State::AddressAccessor& a, const score::DocumentContext& ctx)
{
  settle(a.address, ctx);
}

void settle(::State::Expression& e, const score::DocumentContext& ctx)
{
  auto member = [&](::State::RelationMember& m) {
    if(auto a = m.target<::State::Address>())
      settle(*a, ctx);
    else if(auto a = m.target<::State::AddressAccessor>())
      settle(*a, ctx);
  };
  if(auto rel = e.target<::State::Relation>())
  {
    member(rel->lhs);
    member(rel->rhs);
  }
  else if(auto pulse = e.target<::State::Pulse>())
  {
    settle(pulse->address, ctx);
  }
  for(auto& child : e)
    settle(child, ctx);
}

bool settle(Process::MessageNode& node, const score::DocumentContext& ctx)
{
  bool changed = false;
  if(node.hasValue())
  {
    auto acc = Process::address(node);
    settle(acc, ctx);
    if(acc.address.anchor != node.anchor)
    {
      node.anchor = acc.address.anchor;
      changed = true;
    }
  }
  for(auto& child : node)
    changed |= settle(child, ctx);
  return changed;
}

std::optional<bool> published(const ::State::Address& a, const score::DocumentContext& ctx)
{
  auto t = tree(ctx);
  if(!t)
    return std::nullopt;
  auto& dev = t->device();
  if(a.device.toStdString() != dev.get_name())
    return std::nullopt;
  auto node = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  return node && !ossia::net::get_zombie(*node);
}

AddressStatus addressStatus(const ::State::Address& a, const score::DocumentContext& ctx)
{
  if(a.device.isEmpty() || ossia::traversal::is_pattern(a.toString().toStdString()))
    return AddressStatus::Found;

  if(auto p = published(a, ctx))
    return *p ? AddressStatus::Found : AddressStatus::Unpublished;

  auto provider = ctx.app.interfaces<Device::DeviceModelProviderList>().getBestProvider(ctx);
  auto explorer = provider ? provider->getNodeModel(ctx) : nullptr;
  if(!explorer)
    return AddressStatus::Found;

  auto& root = explorer->rootNode();
  auto dev = ossia::find_if(root, [&](const Device::Node& n) {
    return n.is<Device::DeviceSettings>() && n.get<Device::DeviceSettings>().name == a.device;
  });
  if(dev == root.end())
    return AddressStatus::NoDevice;
  return Device::try_getNodeFromString(*dev, a.path) ? AddressStatus::Found
                                                      : AddressStatus::NoNode;
}

QString describe(AddressStatus s)
{
  switch(s)
  {
    case AddressStatus::Found:
      return {};
    case AddressStatus::Unpublished:
      return QObject::tr("Nothing is published at this address");
    case AddressStatus::NoDevice:
      return QObject::tr("No device of this name in the device explorer");
    case AddressStatus::NoNode:
      return QObject::tr("The device has no such address");
  }
  return {};
}

QObject* publishedObject(
    const ::State::Address& a, const score::DocumentContext& ctx, QString& member)
{
  auto t = tree(ctx);
  if(!t)
    return nullptr;
  auto& dev = t->device();
  if(a.device.toStdString() != dev.get_name())
    return nullptr;
  auto node = ossia::net::find_node(dev.get_root_node(), a.path.join('/').toStdString());
  if(!node || ossia::net::get_zombie(*node))
    return nullptr;
  return t->objectAt(*node, member);
}

bool isProcessState(const ::State::Address& a, const score::DocumentContext& ctx)
{
  QString member;
  return publishedObject(a, ctx, member) && member == QStringLiteral("state");
}

bool broken(const ::State::Address& a, const score::DocumentContext& ctx)
{
  return addressStatus(a, ctx) != AddressStatus::Found;
}

void mapAnchors(Process::MessageNode& n, const AnchorMapping& f)
{
  if(n.anchor)
    n.anchor = f(n.anchor);
  for(auto& child : n)
    mapAnchors(child, f);
}

void mapAnchors(::State::Expression& e, const AnchorMapping& f)
{
  auto list = ::State::anchors(e);
  for(auto& a : list)
    if(a)
      a = f(a);
  ::State::setAnchors(e, list);
}

void mapAddresses(::State::Expression& e, const AddressMapping& map, bool& changed)
{
  auto member = [&](::State::RelationMember& m) {
    if(auto a = m.target<::State::Address>())
    {
      if(auto n = map(::State::AddressAccessor{*a}))
      {
        *a = n->address;
        changed = true;
      }
    }
    else if(auto a = m.target<::State::AddressAccessor>())
    {
      if(auto n = map(*a))
      {
        *a = *n;
        changed = true;
      }
    }
  };
  if(auto rel = e.target<::State::Relation>())
  {
    member(rel->lhs);
    member(rel->rhs);
  }
  else if(auto pulse = e.target<::State::Pulse>())
  {
    if(auto n = map(::State::AddressAccessor{pulse->address}))
    {
      pulse->address = n->address;
      changed = true;
    }
  }
  for(auto& child : e)
    mapAddresses(child, map, changed);
}

::State::Address derive(const ::State::Anchor& a, const score::DocumentContext& ctx)
{
  auto t = tree(ctx);
  if(!t)
    return {};
  auto obj = a.resolve(ctx);
  if(!obj)
    return {};
  if(auto node = t->nodeOf(*obj, a.member))
    return addressOfNode(*node);
  return {};
}

::State::Address derived(const ::State::Address& a, const score::DocumentContext& ctx)
{
  if(!a.anchor)
    return a;
  auto res = derive(*a.anchor, ctx);
  if(!res.isSet())
    return a;
  res.anchor = a.anchor;
  return res;
}

::State::AddressAccessor
derived(const ::State::AddressAccessor& a, const score::DocumentContext& ctx)
{
  ::State::AddressAccessor res = a;
  res.address = derived(a.address, ctx);
  return res;
}
}
