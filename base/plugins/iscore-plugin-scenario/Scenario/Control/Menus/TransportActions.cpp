#include "TransportActions.hpp"

#include "iscore/menu/MenuInterface.hpp"
#include <Scenario/Control/ScenarioControl.hpp>

TransportActions::TransportActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioControl* parent) :
    ScenarioActions{menuElt, parent}
{
    m_play = new QAction{tr("▶ Play"), parent};
    m_play->setObjectName("Play");
    m_play->setShortcut(Qt::Key_Space);
    m_play->setShortcutContext(Qt::ApplicationShortcut);

    m_stop = new QAction{tr("⬛ Stop"), parent};
    m_stop->setObjectName("Stop");
    m_stop->setShortcut(Qt::Key_Return);
    m_stop->setShortcutContext(Qt::ApplicationShortcut);
    m_goToStart = new QAction{tr("⏮ Start"), parent};
    m_goToStart->setObjectName("Start");
    m_goToEnd = new QAction{tr("⏭ End"), parent};
    m_goToEnd->setObjectName("End");
    m_record = new QAction{tr("⚫ Record"), parent};
    m_record->setObjectName("Record");

    m_play->setCheckable(true);
    m_record->setCheckable(true);

    connect(m_play, &QAction::toggled, this, [&] (bool b) {
        m_play->setText(b? QString("❚❚ Pause") : QString("▶ Play"));
    });
    connect(m_stop, &QAction::triggered, this, [&] {
        m_play->blockSignals(true);
        m_record->blockSignals(true);

        m_play->setChecked(false);
        m_play->setText(QString("▶ Play"));
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
        m_record
    };
}

void TransportActions::stop()
{
    m_stop->trigger();
}

