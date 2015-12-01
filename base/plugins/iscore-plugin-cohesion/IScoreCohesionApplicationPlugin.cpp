#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <QAction>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>

#include "Actions/CreateCurves.hpp"
#include "Actions/SnapshotParameters.hpp"
#include "IScoreCohesionApplicationPlugin.hpp"
#include "Record/RecordManager.hpp"
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>

namespace iscore {
class Application;
}  // namespace iscore

IScoreCohesionApplicationPlugin::IScoreCohesionApplicationPlugin(iscore::Application& app) :
    iscore::GUIApplicationContextPlugin {app, "IScoreCohesionApplicationPlugin", nullptr}
{
    // Since we have declared the dependency, we can assume
    // that ScenarioApplicationPlugin is instantiated already.

    iscore::ApplicationContext ctx{app};
    auto& appPlugin = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
    connect(&appPlugin, &ScenarioApplicationPlugin::startRecording,
            this, &IScoreCohesionApplicationPlugin::record);
    connect(&appPlugin, &ScenarioApplicationPlugin::stopRecording, // TODO this seems useless
            this, &IScoreCohesionApplicationPlugin::stopRecord);


    auto acts = appPlugin.actions();
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
            SnapshotParametersInStates(doc->context());
    });

    m_curves = new QAction {tr("Create Curves"), this};
    m_curves->setShortcutContext(Qt::ApplicationShortcut);
    m_curves->setShortcut(tr("Ctrl+L"));
    m_curves->setToolTip(tr("Ctrl+L"));
    connect(m_curves, &QAction::triggered,
            this, [&] () {
        if(auto doc = currentDocument())
            CreateCurves(doc->context());
    });

}

void IScoreCohesionApplicationPlugin::populateMenus(iscore::MenubarManager* menu)
{
    // If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
    // We can add an option in the menu to generate curves from the selected addresses
    // in the current constraint.

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_curves);

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_snapshot);

}

std::vector<iscore::OrderedToolbar> IScoreCohesionApplicationPlugin::makeToolbars()
{
    QToolBar* bar = new QToolBar;
    bar->addActions({m_curves, m_snapshot});
    return std::vector<iscore::OrderedToolbar>{iscore::OrderedToolbar(2, bar)};
}

void IScoreCohesionApplicationPlugin::record(Scenario::ScenarioModel& scenar, Scenario::Point pt)
{
    m_recManager = std::make_unique<RecordManager>();
    m_recManager->recordInNewBox(scenar, pt);
}

void IScoreCohesionApplicationPlugin::stopRecord()
{
    if(m_recManager)
    {
        m_recManager->stopRecording();
        m_recManager.release();
    }
}
