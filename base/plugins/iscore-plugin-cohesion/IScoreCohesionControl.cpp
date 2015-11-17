#include "IScoreCohesionControl.hpp"

#include "Record/RecordManager.hpp"

#include "Actions/CreateCurves.hpp"
#include "Actions/SnapshotParameters.hpp"

#include <iscore/menu/MenuInterface.hpp>
#include <Scenario/Control/ScenarioControl.hpp>
#include <core/application/Application.hpp>
#include <QApplication>
#include <QToolBar>

IScoreCohesionControl::IScoreCohesionControl(iscore::Application& app) :
    iscore::PluginControlInterface {app, "IScoreCohesionControl", nullptr}
{
    // Since we have declared the dependency, we can assume
    // that ScenarioControl is instantiated already.

    iscore::ApplicationContext ctx{app};
    auto scen = ctx.components.control<ScenarioControl>();
    connect(scen, &ScenarioControl::startRecording,
            this, &IScoreCohesionControl::record);
    connect(scen, &ScenarioControl::stopRecording, // TODO this seems useless
            this, &IScoreCohesionControl::stopRecord);


    auto acts = scen->actions();
    for(const auto& act : acts)
    {
        if(act->objectName() == "Stop")
        {
            m_stopAction = act;
            connect(m_stopAction, &QAction::triggered,
                    this, [&] {
                stopRecord();
            });
        }
    }

    m_snapshot = new QAction {tr("Snapshot in Event"), this};
    m_snapshot->setShortcutContext(Qt::ApplicationShortcut);
    m_snapshot->setShortcut(tr("Ctrl+J"));
    m_snapshot->setToolTip(tr("Ctrl+J"));
    connect(m_snapshot, &QAction::triggered,
            this, [&] () {
        if(auto doc = currentDocument())
            SnapshotParametersInStates(*doc);
    });

    m_curves = new QAction {tr("Create Curves"), this};
    m_curves->setShortcutContext(Qt::ApplicationShortcut);
    m_curves->setShortcut(tr("Ctrl+L"));
    m_curves->setToolTip(tr("Ctrl+L"));
    connect(m_curves, &QAction::triggered,
            this, [&] () {
        if(auto doc = currentDocument())
            CreateCurves(*doc);
    });

}

void IScoreCohesionControl::populateMenus(iscore::MenubarManager* menu)
{
    // If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
    // We can add an option in the menu to generate curves from the selected addresses
    // in the current constraint.

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_curves);

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_snapshot);

}

QList<iscore::OrderedToolbar> IScoreCohesionControl::makeToolbars()
{
    QToolBar* bar = new QToolBar;
    bar->addActions({m_curves, m_snapshot});
    return QList<iscore::OrderedToolbar>{iscore::OrderedToolbar(2, bar)};
}

void IScoreCohesionControl::record(ScenarioModel& scenar, Scenario::Point pt)
{
    m_recManager = std::make_unique<RecordManager>();
    m_recManager->recordInNewBox(scenar, pt);
}

void IScoreCohesionControl::stopRecord()
{
    if(m_recManager)
    {
        m_recManager->stopRecording();
        m_recManager.release();
    }
}
