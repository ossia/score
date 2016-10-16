#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Curve/Palette/CurvePoint.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace State {
struct Address;
}  // namespace iscore

namespace Automation
{
class ProcessModel;
class ChangeAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), ChangeAddress, "ChangeAddress")
    public:
        ChangeAddress(
                Path<ProcessModel>&& path,
                const State::AddressAccessor& newval);
        ChangeAddress(
                Path<ProcessModel>&& path,
                Device::FullAddressSettings newval);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<ProcessModel> m_path;
        State::AddressAccessor m_oldAddress, m_newAddress;
        Device::Domain m_oldDomain, m_newDomain;
};

}
