#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

class AddDevice : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AddDevice", "AddDevice")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddDevice, "DeviceExplorerControl")
        AddDevice(ObjectPath&& device_tree,
                  const iscore::DeviceSettings& parameters,
                  const QString& filePath = QString(""));


        virtual void undo() override;
        virtual void redo() override;

        // After redo(), contains the row of the added device.
        // TODO how to precompute this ?
        int deviceRow() const;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_deviceTree;
        iscore::DeviceSettings m_parameters;
        QString m_filePath{};

        int m_row{-1};
};
