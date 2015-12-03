#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class AutomationModel;
class DataStreamInput;
class DataStreamOutput;
namespace iscore {
struct Address;
}  // namespace iscore

class ChangeAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(AutomationCommandFactoryName(), ChangeAddress, "ChangeAddress")
    public:
        ChangeAddress(
                Path<AutomationModel>&& path,
                const iscore::Address& newval);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<AutomationModel> m_path;
        iscore::FullAddressSettings m_old, m_new;
};


