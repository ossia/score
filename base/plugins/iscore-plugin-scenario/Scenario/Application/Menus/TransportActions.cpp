#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <QAction>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include "TransportActions.hpp"
#include <core/presenter/MenubarManager.hpp>
#include <iscore/menu/MenuInterface.hpp>

#include <iscore/widgets/SetIcons.hpp>

class QMenu;

namespace Scenario
{
class TemporalScenarioPresenter;

TransportActions::TransportActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioApplicationPlugin* parent) :
    ScenarioActions{menuElt, parent}
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

void TransportActions::fillMenuBar(iscore::MenubarManager *menu)
{
    for(auto act : actions())
    {
        menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::PlayMenu, act);
    }
}

void TransportActions::fillContextMenu(QMenu *menu, const Selection& sel, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&)
{

}

bool TransportActions::populateToolBar(QToolBar *bar)
{
    bar->addActions(actions());
    return true;
}

void TransportActions::setEnabled(bool)
{

}

QList<QAction*> TransportActions::actions() const
{
    return {
        m_play,
        m_stop,
        //m_goToStart,
        //m_goToEnd,
        m_stopAndInit
        //m_record
    };
}

void TransportActions::stop()
{
    m_stop->trigger();
}
}
