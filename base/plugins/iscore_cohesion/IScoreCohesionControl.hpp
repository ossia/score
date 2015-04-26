#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "FakeEngine.hpp"
#include <QTemporaryFile>

class IScoreCohesionControl : public iscore::PluginControlInterface
{
    public:
        IScoreCohesionControl(QObject* parent);
        void populateMenus(iscore::MenubarManager*) override;
        iscore::SerializableCommand* instantiateUndoCommand(const QString& name, const QByteArray& data) override;

    public slots:
        void on_currentTimeChanged(double);

        void createCurvesFromAddresses();
        void snapshotParametersInEvents();

    private:
        FakeEngine m_engine;
        QTemporaryFile m_scoreFile;
};
