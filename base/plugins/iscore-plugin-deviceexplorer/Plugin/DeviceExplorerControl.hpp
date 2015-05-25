#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
namespace iscore
{
class SerializableCommand;
}
class DeviceExplorerControl : public iscore::PluginControlInterface
{
    public:
        DeviceExplorerControl(iscore::Presenter*);

        iscore::SerializableCommand*
        instantiateUndoCommand(const QString & name,
                               const QByteArray & arr) override;

    protected:
        void on_newDocument(iscore::Document* doc) override;
        void on_documentChanged() override;
};
