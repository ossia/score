#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <QAction>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include "TransportActions.hpp"
#include <core/presenter/MenubarManager.hpp>
#include <iscore/menu/MenuInterface.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <iscore/menu/MenuInterface.hpp>

class QMenu;

namespace Scenario
{
class TemporalScenarioPresenter;

TransportActions::TransportActions(
        ScenarioApplicationPlugin* parent) :
    m_parent{parent}
{
    m_play = new QAction{tr("Play"), this};
    m_play->setObjectName("Play");
    m_play->setShortcut(Qt::Key_Space);
    m_play->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    setIcons(m_play, QString(":/icons/play_on.png"), QString(":/icons/play_off.png"));

    m_stop = new QAction{tr("Stop"), this};
    m_stop->setObjectName("Stop");
    m_stop->setShortcut(Qt::Key_Return);
    m_stop->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    setIcons(m_stop, QString(":/icons/stop_on.png"), QString(":/icons/stop_off.png"));

    m_goToStart = new QAction{tr("⏮ Start"), this};
    m_goToStart->setObjectName("Start");
    setIcons(m_goToStart, QString(":/icons/start_on.png"), QString(":/icons/start_off.png"));

    m_goToEnd = new QAction{tr("⏭ End"), this};
    m_goToEnd->setObjectName("End");
    setIcons(m_goToEnd, QString(":/icons/end_on.png"), QString(":/icons/end_off.png"));

    m_stopAndInit = new QAction{tr("Reinitialize"), this};
    m_stopAndInit->setObjectName("StopAndInit");
    m_stopAndInit->setShortcut(Qt::CTRL + Qt::Key_Return);
    m_stopAndInit->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    setIcons(m_stopAndInit, QString(":/icons/reinitialize_on.png"), QString(":/icons/reinitialize_off.png"));

    m_record = new QAction{tr("Record"), this};
    m_record->setObjectName("Record");
    setIcons(m_record, QString(":/icons/record_on.png"), QString(":/icons/record_off.png"));

    m_play->setCheckable(true);
    m_record->setCheckable(true);

    connect(m_play, &QAction::toggled, this, [&] (bool b) {
        m_play->setText(b? QString("Pause") : QString("Play"));
        setIcons(m_play,
                 b ? QString(":/icons/pause_on.png") : QString(":/icons/play_on.png"),
                 b ? QString(":/icons/pause_off.png") : QString(":/icons/play_off.png"));
    });
    connect(m_stop, &QAction::triggered, this, [&] {
        m_play->blockSignals(true);
        m_record->blockSignals(true);

        m_play->setChecked(false);
        m_play->setText(QString("Play"));
        m_record->setChecked(false);

        m_play->blockSignals(false);
        m_record->blockSignals(false);
    });
    connect(m_goToStart, &QAction::triggered, this, [&] {
        m_play->blockSignals(true);
        m_record->blockSignals(true);

        m_play->setChecked(false);
        m_record->setChecked(false);

        m_play->blockSignals(false);
        m_record->blockSignals(false);
    });
    connect(m_goToEnd, &QAction::triggered, this, [&] {
        m_play->blockSignals(true);
        m_record->blockSignals(true);

        m_play->setChecked(false);
        m_record->setChecked(false);

        m_play->blockSignals(false);
        m_record->blockSignals(false);
    });
    connect(m_stopAndInit, &QAction::triggered,
            m_stop, &QAction::trigger);
    connect(m_record, &QAction::toggled, this, [&] (bool b) {
    });
}

void TransportActions::makeGUIElements(iscore::GUIElements& ref)
{
    auto& cond = m_parent->context.actions.condition<iscore::EnableWhenDocumentIs<Scenario::ScenarioDocumentModel>>();

    // Put m_play m_stop and m_stopAndInit only for now in their own toolbar,
    // plus everything in the play menu
    {
        auto bar = new QToolBar{tr("Transport")};
        bar->addAction(m_play);
        bar->addAction(m_stop);
        bar->addAction(m_stopAndInit);

        ref.toolbars.emplace_back(bar, StringKey<iscore::Toolbar>("Transport"), 1, 0);
    }

    {
        auto& play = m_parent->context.menus.get().at(iscore::Menus::Play());
        play.menu()->addAction(m_play);
        play.menu()->addAction(m_stop);
        play.menu()->addAction(m_stopAndInit);
    }

    ref.actions.add<Actions::Play>(m_play);
    ref.actions.add<Actions::Stop>(m_stop);
    ref.actions.add<Actions::GoToStart>(m_goToStart);
    ref.actions.add<Actions::GoToEnd>(m_goToEnd);
    ref.actions.add<Actions::Reinitialize>(m_stopAndInit);
    ref.actions.add<Actions::Record>(m_record);

    cond.add<Actions::Play>();
    cond.add<Actions::Stop>();
    cond.add<Actions::GoToStart>();
    cond.add<Actions::GoToEnd>();
    cond.add<Actions::Reinitialize>();
    cond.add<Actions::Record>();
}

}
