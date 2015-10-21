#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <QTemporaryFile>
#include <QTimer>
#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"
class RecordManager;
class ScenarioModel;
class IScoreCohesionControl : public iscore::PluginControlInterface
{
    public:
        IScoreCohesionControl(iscore::Presenter* pres);
        void populateMenus(iscore::MenubarManager*) override;
        QList<iscore::OrderedToolbar> makeToolbars() override;

        iscore::SerializableCommand* instantiateUndoCommand(
                const QString& name,
                const QByteArray& data) override;

        void record(ScenarioModel&, ScenarioPoint pt);
        void stopRecord();

    private:
        void setupCommands();

        QAction* m_snapshot{};
        QAction* m_curves{};

        std::unique_ptr<RecordManager> m_recManager;
};
