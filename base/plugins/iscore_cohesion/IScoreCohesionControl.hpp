#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "FakeEngine.hpp"
#include <QTemporaryFile>

class IScoreCohesionControl : public iscore::PluginControlInterface
{
    public:
        IScoreCohesionControl(QObject* parent);
        void populateMenus(iscore::MenubarManager*) override;
        QList<QToolBar*> makeToolbars() override;

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
        QTemporaryFile m_scoreFile;

        QAction* m_snapshot{};
        QAction* m_curves{};
        QAction* m_interp{};
};
