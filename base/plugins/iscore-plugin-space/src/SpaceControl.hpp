#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class SpaceControl : public iscore::PluginControlInterface
{
    public:
        SpaceControl(iscore::Presenter* pres);
        virtual ~SpaceControl() = default;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                                                                    const QByteArray& data) override;
    private:
        void setupCommands();
};
