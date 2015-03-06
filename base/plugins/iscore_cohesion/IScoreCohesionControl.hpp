#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>

class IScoreCohesionControl : public iscore::PluginControlInterface
{
    public:
        IScoreCohesionControl(QObject* parent);
        void populateMenus(iscore::MenubarManager*) override;
        void populateToolbars() override { }
        iscore::SerializableCommand* instantiateUndoCommand(const QString& name, const QByteArray& data) override;

    public slots:
        void createCurvesFromAddresses();
};
