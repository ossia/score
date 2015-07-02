#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <QTemporaryFile>

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
        void createCurvesFromAddresses();
        void snapshotParametersInEvents();
        void interpolateStates();

    private:
        QAction* m_snapshot{};
        QAction* m_curves{};
        QAction* m_interp{};
};
