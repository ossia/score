#include "IScoreCohesionControl.hpp"

#include "Record/RecordManager.hpp"

#include "Actions/CreateCurves.hpp"
#include "Actions/SnapshotParameters.hpp"

#include "Commands/CreateCurvesFromAddresses.hpp"
#include "Commands/CreateCurvesFromAddressesInConstraints.hpp"
#include "Commands/CreateStatesFromParametersInEvents.hpp"
#include "Commands/Record.hpp"


#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <Control/ScenarioControl.hpp>

#include <QApplication>
#include <QToolBar>

IScoreCohesionControl::IScoreCohesionControl(iscore::Presenter* pres) :
    iscore::PluginControlInterface {pres, "IScoreCohesionControl", nullptr}
{
    // Since we have declared the dependency, we can assume
    // that ScenarioControl is instantiated already.
    auto scen = ScenarioControl::instance();
    connect(scen, &ScenarioControl::startRecording,
            this, &IScoreCohesionControl::record);
    connect(scen, &ScenarioControl::stopRecording,
            this, &IScoreCohesionControl::stopRecord);


    auto acts = scen->actions();
    for(const auto& act : acts)
    {
        if(act->objectName() == "Stop")
        {
            connect(act, &QAction::triggered,
                    this, [&] {
                stopRecord();
            });
        }
    }

    setupCommands();

    m_snapshot = new QAction {tr("Snapshot in Event"), this};
    m_snapshot->setShortcutContext(Qt::ApplicationShortcut);
    m_snapshot->setShortcut(tr("Ctrl+J"));
    m_snapshot->setToolTip(tr("Ctrl+J"));
    connect(m_snapshot, &QAction::triggered,
            this, [&] () {
        SnapshotParametersInStates(currentDocument());
    });

    m_curves = new QAction {tr("Create Curves"), this};
    m_curves->setShortcutContext(Qt::ApplicationShortcut);
    m_curves->setShortcut(tr("Ctrl+L"));
    m_curves->setToolTip(tr("Ctrl+L"));
    connect(m_curves, &QAction::triggered,
            this, [&] () {
        CreateCurves(currentDocument());
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

namespace {
struct IScoreCohesionCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap IScoreCohesionCommandFactory::map;
}

void IScoreCohesionControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
            CreateCurvesFromAddresses,
            CreateCurvesFromAddressesInConstraints,
            Record,
            SnapshotStatesMacro
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter<IScoreCohesionCommandFactory>());
}

void IScoreCohesionControl::record(ScenarioModel& scenar, ScenarioPoint pt)
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

