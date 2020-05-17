#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace State
{
struct Address;
} // namespace score

namespace Mapping
{
class ProcessModel;
class ChangeSourceAddress final : public score::Command
{
  SCORE_COMMAND_DECL(MappingCommandFactoryName(), ChangeSourceAddress, "ChangeSourceAddress")
public:
  ChangeSourceAddress(const ProcessModel&, Device::FullAddressAccessorSettings newval);

public:
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  Device::FullAddressAccessorSettings m_old, m_new;
};

class ChangeTargetAddress final : public score::Command
{
  SCORE_COMMAND_DECL(MappingCommandFactoryName(), ChangeTargetAddress, "ChangeTargetAddress")
public:
  ChangeTargetAddress(const ProcessModel&, Device::FullAddressAccessorSettings);

public:
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  Device::FullAddressAccessorSettings m_old, m_new;
};
}
