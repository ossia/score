#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

class AddDevice : public iscore::SerializableCommand
{
        ISCORE_COMMAND
        public:
            ISCORE_COMMAND_DEFAULT_CTOR(AddDevice, "DeviceExplorerControl")
        AddDevice(ObjectPath&& device_tree,
                  const DeviceSettings& parameters);

        virtual void undo() override;
        virtual void redo() override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_deviceTree;
        DeviceSettings m_parameters;

        int m_row{};
};
