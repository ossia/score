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

#include <core/document/Document.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>
#include <Curve/Settings/Model.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <QApplication>
ISCORE_DECLARE_ACTION(Snapshot, Scenario, QKeySequence(QObject::tr("Ctrl+L")))
ISCORE_DECLARE_ACTION(CreateCurves, Scenario, QKeySequence(QObject::tr("Ctrl+J")))

IScoreCohesionApplicationPlugin::IScoreCohesionApplicationPlugin(
        const iscore::GUIApplicationContext& ctx) :
    iscore::GUIApplicationContextPlugin {ctx}
{
    using namespace Scenario;
    // Since we have declared the dependency, we can assume
    // that ScenarioApplicationPlugin is instantiated already.
    auto& scenario_plugin = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
    connect(&scenario_plugin.execution(), &ScenarioExecution::startRecording,
            this, &IScoreCohesionApplicationPlugin::record);
    connect(&scenario_plugin.execution(), &ScenarioExecution::startRecordingMessages,
            this, &IScoreCohesionApplicationPlugin::recordMessages);
    connect(&scenario_plugin.execution(), &ScenarioExecution::stopRecording, // TODO this seems useless
            this, &IScoreCohesionApplicationPlugin::stopRecord);


    m_ossiaplug = &ctx.components.applicationPlugin<OSSIAApplicationPlugin>();

    auto& stop_action = ctx.actions.action<Actions::Stop>();
    m_stopAction = stop_action.action();
    connect(m_stopAction, &QAction::triggered,
            this, [&] { stopRecord(); });

    m_snapshot = new QAction {tr("Snapshot in Event"), this};
    m_snapshot->setShortcutContext(Qt::ApplicationShortcut);
    m_snapshot->setShortcut(tr("Ctrl+J"));
    m_snapshot->setToolTip(tr("Snapshot in Event (Ctrl+J)"));

    setIcons(m_snapshot, QString(":/icons/snapshot_on.png"), QString(":/icons/snapshot_off.png"));

    connect(m_snapshot, &QAction::triggered,
            this, [&] () {
        if(auto doc = currentDocument())
            SnapshotParametersInStates(doc->context());
    });
    m_snapshot->setEnabled(false);

    m_curves = new QAction {tr("Create Curves"), this};
    m_curves->setShortcutContext(Qt::ApplicationShortcut);
    m_curves->setShortcut(tr("Ctrl+L"));
    m_curves->setToolTip(tr("Create Curves (Ctrl+L)"));

    setIcons(m_curves, QString(":/icons/create_curve_on.png"), QString(":/icons/create_curve_off.png"));

    connect(m_curves, &QAction::triggered,
            this, [&] () {
        if(auto doc = currentDocument())
            DoForSelectedConstraints(doc->context(), CreateCurves);
    });
    m_curves->setEnabled(false);

}

iscore::GUIElements IScoreCohesionApplicationPlugin::makeGUIElements()
{
    using namespace iscore;

    GUIElements e;

    Menu& object = context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_snapshot);
    object.menu()->addAction(m_curves);
    {
        iscore::Toolbar& bar = context.toolbars.get().at(StringKey<iscore::Toolbar>("Constraint"));
        bar.toolbar()->addAction(m_snapshot);
        bar.toolbar()->addAction(m_curves);
    }

    e.actions.add<Actions::Snapshot>(m_snapshot);
    e.actions.add<Actions::CreateCurves>(m_curves);

    auto& cstr_cond = context.actions.condition<iscore::EnableWhenSelectionContains<Scenario::ConstraintModel>>();
    auto& state_cond = context.actions.condition<iscore::EnableWhenSelectionContains<Scenario::StateModel>>();

    state_cond.add<Actions::Snapshot>();
    cstr_cond.add<Actions::CreateCurves>();

    return e;
}

void IScoreCohesionApplicationPlugin::record(
        const Scenario::ScenarioModel& scenar,
        Scenario::Point pt)
{
    m_stopAction->trigger();
    QApplication::processEvents();

    m_recManager = std::make_unique<Recording::RecordManager>(
                iscore::IDocument::documentContext(scenar));

    if(context.settings<Curve::Settings::Model>().getPlayWhileRecording())
    {
        connect(m_recManager.get(), &Recording::RecordManager::requestPlay,
                this, [=] ()
        {
            m_ossiaplug->on_record(pt.date);
        }, Qt::QueuedConnection);
    }

    m_recManager->recordInNewBox(scenar, pt);

}

void IScoreCohesionApplicationPlugin::recordMessages(
        const Scenario::ScenarioModel& scenar,
        Scenario::Point pt)
{
    m_stopAction->trigger();
    QApplication::processEvents();

    m_recMessagesManager = std::make_unique<Recording::RecordMessagesManager>(
                iscore::IDocument::documentContext(scenar));

    if(context.settings<Curve::Settings::Model>().getPlayWhileRecording())
    {
        connect(m_recMessagesManager.get(), &Recording::RecordMessagesManager::requestPlay,
                this, [=] ()
        {
            m_ossiaplug->on_record(pt.date);
        }, Qt::QueuedConnection);
    }

    m_recMessagesManager->recordInNewBox(scenar, pt);
}

void IScoreCohesionApplicationPlugin::stopRecord()
{
    if(m_recManager)
    {
        m_recManager->stopRecording();
        m_recManager.reset();
    }

    if(m_recMessagesManager)
    {
        m_recMessagesManager->stopRecording();
        m_recMessagesManager.reset();
    }
}
