#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class MappingModel;
namespace iscore {
struct Address;
}  // namespace iscore

class ChangeSourceAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), ChangeSourceAddress, "ChangeSourceAddress")
    public:
        ChangeSourceAddress(
                Path<MappingModel>&& path,
                const iscore::Address& newval);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<MappingModel> m_path;
        iscore::FullAddressSettings m_old, m_new;
};

class ChangeTargetAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), ChangeTargetAddress, "ChangeTargetAddress")
    public:
        ChangeTargetAddress(
                Path<MappingModel>&& path,
                const iscore::Address& newval);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<MappingModel> m_path;
        iscore::FullAddressSettings m_old, m_new;
};



