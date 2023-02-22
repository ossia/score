#pragma once
#include <State/Message.hpp>

#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <ControlSurface/Metadata.hpp>

#include <ossia/detail/hash_map.hpp>

#include <unordered_map>

namespace Device
{
struct FullAddressAccessorSettings;
}
namespace Process
{
class ControlInlet;
}

namespace ControlSurface
{
template <typename Identifier, typename Func>
struct NodeObserver : public QObject
{
  Explorer::DeviceExplorerModel& model;
  Func f;

  NodeObserver(Explorer::DeviceExplorerModel& explorer, Func fun)
      : model{explorer}
      , f{fun}
  {
    // TODO handle the case where the node isn't available / gets removed and added again...
    connect(
        &explorer, &Explorer::DeviceExplorerModel::nodeChanged, this,
        [this](Device::Node* n) {
      if(auto it = available.find(n); it != available.end())
      {
        f(n, it->second.id);
      }
        });
  }

  void listen(State::Address addr, Identifier identifier)
  {
    auto node = Device::try_getNodeFromAddress(model.rootNode(), addr);
    if(node)
    {
      available[node] = {std::move(addr), std::move(identifier)};
    }
    else
    {
      missing.push_back({std::move(addr), std::move(identifier)});
    }
  }

  void unlisten(const State::Address& addr)
  {
    auto it = ossia::find_if(
        available, [&](const auto& pair) { return pair.second.accessor == addr; });
    if(it != available.end())
    {
      available.erase(it);
    }
    else
    {
      auto it
          = ossia::find_if(missing, [&](const auto& p) { return p.accessor == addr; });
      if(it != missing.end())
        missing.erase(it);
    }
  }

  struct AvailableNode
  {
    State::Address accessor;
    Identifier id;
  };

  std::unordered_map<Device::Node*, AvailableNode> available;
  std::vector<AvailableNode> missing;
};

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(ControlSurface::Model)
  W_OBJECT(Model)

public:
  using address_map = ossia::hash_map<int32_t, State::AddressAccessor>;
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
      , m_observer{Explorer::deviceExplorerFromObject(*parent), Apply{*this}}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  using Process::ProcessModel::inlets;
  using Process::ProcessModel::outlets;

  Process::Inlets& inlets() noexcept { return m_inlets; }
  Process::Outlets& outlets() noexcept { return m_outlets; }

  void addControl(
      const Id<Process::Port>& id, const Device::FullAddressAccessorSettings& message);
  void setupControl(Process::ControlInlet* ctl, const State::AddressAccessor& addr);
  void removeControl(const Id<Process::Port>& id);

  const address_map& outputAddresses() const noexcept { return m_outputAddresses; }

private:
  QString prettyName() const noexcept override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  address_map m_outputAddresses;

  struct Apply
  {
    ProcessModel& model;
    void operator()(Device::Node*, int);
  };

  NodeObserver<int, Apply> m_observer;
};

using ProcessFactory = Process::ProcessFactory_T<ControlSurface::Model>;
}
