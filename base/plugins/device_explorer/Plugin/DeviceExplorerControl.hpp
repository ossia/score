#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
namespace iscore
{
class SerializableCommand;
}
class DeviceExplorerControl : public iscore::PluginControlInterface
{
    public:
        DeviceExplorerControl();

        iscore::SerializableCommand*
        instantiateUndoCommand(const QString & name,
                               const QByteArray & arr) override;
};
