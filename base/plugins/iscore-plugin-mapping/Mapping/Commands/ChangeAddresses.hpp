#pragma once
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <Device/Address/AddressSettings.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/command/SerializableCommand.hpp>

class MappingModel;
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
        void serializeImpl(QDataStream &) const override;
        void deserializeImpl(QDataStream &) override;

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
        void serializeImpl(QDataStream &) const override;
        void deserializeImpl(QDataStream &) override;

    private:
        Path<MappingModel> m_path;
        iscore::FullAddressSettings m_old, m_new;
};



