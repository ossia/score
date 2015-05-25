#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "FakeEngine.hpp"
#include <QTemporaryFile>

class Executor;
class IScoreCohesionControl : public iscore::PluginControlInterface
{
    public:
        IScoreCohesionControl(iscore::Presenter* pres);
        void populateMenus(iscore::MenubarManager*) override;
        QList<OrderedToolbar> makeToolbars() override;

        iscore::SerializableCommand* instantiateUndoCommand(
                const QString& name,
                const QByteArray& data) override;

    public slots:
        void on_currentTimeChanged(double);

        void createCurvesFromAddresses();
        void snapshotParametersInEvents();
        void interpolateStates();

    private:
        FakeEngine m_engine;
        Executor* m_executor{};
        QTemporaryFile m_scoreFile;

        QAction* m_snapshot{};
        QAction* m_curves{};
        QAction* m_interp{};
};
