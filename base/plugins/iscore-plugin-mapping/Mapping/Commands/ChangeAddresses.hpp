#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace State
{
struct Address;
} // namespace iscore

namespace Mapping
{
class ProcessModel;
class ChangeSourceAddress final : public iscore::SerializableCommand
{
  ISCORE_COMMAND_DECL(
      MappingCommandFactoryName(), ChangeSourceAddress, "ChangeSourceAddress")
public:
  ChangeSourceAddress(
      const ProcessModel&, Device::FullAddressAccessorSettings newval);

public:
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  Device::FullAddressAccessorSettings m_old, m_new;
};

class ChangeTargetAddress final : public iscore::SerializableCommand
{
  ISCORE_COMMAND_DECL(
      MappingCommandFactoryName(), ChangeTargetAddress, "ChangeTargetAddress")
public:
  ChangeTargetAddress(const ProcessModel&, Device::FullAddressAccessorSettings);

public:
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  Device::FullAddressAccessorSettings m_old, m_new;
};
}
