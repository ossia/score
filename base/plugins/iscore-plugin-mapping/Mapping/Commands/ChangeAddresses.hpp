#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
namespace State {
struct Address;
}  // namespace iscore

namespace Mapping
{
class MappingModel;
class ChangeSourceAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), ChangeSourceAddress, "ChangeSourceAddress")
    public:
        ChangeSourceAddress(
                Path<MappingModel>&& path,
                const State::Address& newval);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<MappingModel> m_path;
        Device::FullAddressSettings m_old, m_new;
};

// TODO break me
class ChangeTargetAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), ChangeTargetAddress, "ChangeTargetAddress")
    public:
        ChangeTargetAddress(
                Path<MappingModel>&& path,
                const State::Address& newval);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<MappingModel> m_path;
        Device::FullAddressSettings m_old, m_new;
};
}
