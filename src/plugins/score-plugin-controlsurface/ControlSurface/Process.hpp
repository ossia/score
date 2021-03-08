#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <State/Message.hpp>

#include <ossia/detail/hash_map.hpp>

#include <ControlSurface/Metadata.hpp>

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
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(ControlSurface::Model)
  W_OBJECT(Model)

public:
  using address_map = ossia::fast_hash_map<int32_t, State::AddressAccessor>;
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  using Process::ProcessModel::inlets;
  using Process::ProcessModel::outlets;

  Process::Inlets& inlets() noexcept { return m_inlets; }
  Process::Outlets& outlets() noexcept { return m_outlets; }

  void addControl(const Id<Process::Port>& id, const Device::FullAddressAccessorSettings& message);
  void setupControl(Process::ControlInlet* ctl, const State::AddressAccessor& addr);
  void removeControl(const Id<Process::Port>& id);

  const address_map& outputAddresses() const noexcept { return m_outputAddresses; }

private:
  QString prettyName() const noexcept override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  address_map m_outputAddresses;
};

using ProcessFactory = Process::ProcessFactory_T<ControlSurface::Model>;
}
