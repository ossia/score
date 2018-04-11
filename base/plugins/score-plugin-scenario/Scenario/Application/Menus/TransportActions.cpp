// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QAction>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <score/actions/ActionManager.hpp>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>

#include "TransportActions.hpp"

#include <score/actions/MenuManager.hpp>

#include <QApplication>
#include <QMainWindow>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/actions/Menu.hpp>
#include <score/widgets/SetIcons.hpp>
#include <core/application/ApplicationSettings.hpp>
class QMenu;

namespace Scenario
{
class TemporalScenarioPresenter;

TransportActions::TransportActions(
    const score::GUIApplicationContext& context)
    : m_context{context}
{
  if(!context.applicationSettings.gui)
    return;
  m_play = new QAction{tr("Play"), nullptr};
  m_play->setCheckable(true);
  m_play->setObjectName("Play");
  m_play->setShortcut(Qt::Key_Space);
  m_play->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_play->setData(false);
  setIcons(
      m_play, QString(":/icons/play_on.png"), QString(":/icons/play_off.png"));

  m_playGlobal = new QAction{tr("Play (Global)"), nullptr};
  m_playGlobal->setCheckable(true);
  m_playGlobal->setObjectName("Play (Global)");
  m_playGlobal->setShortcut(Qt::Key_Shift + Qt::Key_Space);
  m_playGlobal->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_playGlobal->setData(false);
  setIcons(
      m_playGlobal, QString(":/icons/play_glob_on.png"), QString(":/icons/play_glob_off.png"));

  m_stop = new QAction{tr("Stop"), nullptr};
  m_stop->setObjectName("Stop");
  m_stop->setShortcut(Qt::Key_Return);
  m_stop->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  setIcons(
      m_stop, QString(":/icons/stop_on.png"), QString(":/icons/stop_off.png"));
  /*
      m_goToStart = new QAction{tr("⏮ Start"), nullptr};
      m_goToStart->setObjectName("Start");
      setIcons(m_goToStart, QString(":/icons/start_on.png"),
     QString(":/icons/start_off.png"));

      m_goToEnd = new QAction{tr("⏭ End"), nullptr};
      m_goToEnd->setObjectName("End");
      setIcons(m_goToEnd, QString(":/icons/end_on.png"),
     QString(":/icons/end_off.png"));
  */
  m_stopAndInit = new QAction{tr("Reinitialize"), nullptr};
  m_stopAndInit->setObjectName("StopAndInit");
  m_stopAndInit->setShortcut(Qt::CTRL + Qt::Key_Return);
  m_stopAndInit->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  setIcons(
      m_stopAndInit, QString(":/icons/reinitialize_on.png"),
      QString(":/icons/reinitialize_off.png"));
  /*
      m_record = new QAction{tr("Record"), nullptr};
      m_record->setObjectName("Record");
      setIcons(m_record, QString(":/icons/record_on.png"),
     QString(":/icons/record_off.png"));
  */
  //    m_record->setCheckable(true);

  auto on_play = [&](bool b) {
    m_play->setText(b ? tr("Pause") : tr("Play"));
    m_play->setData(b); // True for "pause" state (i.e. currently playing),
                        // false for "play" state (i.e. currently paused)
    setIcons(
        m_play,
        b ? QString(":/icons/pause_on.png") : QString(":/icons/play_on.png"),
        b ? QString(":/icons/pause_off.png")
          : QString(":/icons/play_off.png"));

    m_playGlobal->setText(b ? tr("Pause") : tr("Play (global)"));
    m_playGlobal->setData(b);
    setIcons(
          m_playGlobal,
          b ? QString(":/icons/pause_on.png") : QString(":/icons/play_glob_on.png"),
          b ? QString(":/icons/pause_off.png")
            : QString(":/icons/play_glob_off.png"));
  };
  connect(m_play, &QAction::toggled, this, [=] (bool b) {
    on_play(b);
    m_playGlobal->setEnabled(false);
  });
  connect(m_playGlobal, &QAction::toggled, this, [=] (bool b) {
    on_play(b);
    m_play->setEnabled(false);
  });
  connect(m_stop, &QAction::triggered, this, [&] {
    m_play->blockSignals(true);
    m_playGlobal->blockSignals(true);
    //        m_record->blockSignals(true);

    m_play->setEnabled(true);
    m_play->setChecked(false);
    m_play->setText(tr("Play"));
    setIcons(
        m_play, QString(":/icons/play_on.png"),
        QString(":/icons/play_off.png"));
    m_play->setData(false);

    m_playGlobal->setEnabled(true);
    m_playGlobal->setChecked(false);
    m_play->setText(tr("Play (global)"));
    setIcons(
          m_playGlobal, QString(":/icons/play_glob_on.png"),
          QString(":/icons/play_glob_off.png"));
    m_playGlobal->setData(false);
    //        m_record->setChecked(false);

    m_play->blockSignals(false);
    m_playGlobal->blockSignals(false);
    //        m_record->blockSignals(false);
  });
  /*
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
  */
  connect(m_stopAndInit, &QAction::triggered, m_stop, &QAction::trigger);
  //    connect(m_record, &QAction::toggled, this, [&] (bool b) {
  //    });

  if(context.mainWindow)
  {
    auto obj = context.mainWindow->centralWidget();
    obj->addAction(m_play);
    obj->addAction(m_playGlobal);
    obj->addAction(m_stop);
    obj->addAction(m_stopAndInit);
  }
}

void TransportActions::makeGUIElements(score::GUIElements& ref)
{
  auto& cond
      = m_context.actions
            .condition<score::
                           EnableWhenDocumentIs<Scenario::
                                                    ScenarioDocumentModel>>();

  // Put m_play m_stop and m_stopAndInit only for now in their own toolbar,
  // plus everything in the play menu
  {
    auto bar = new QToolBar{tr("Transport")};
    bar->addAction(m_play);
    bar->addAction(m_playGlobal);
    bar->addAction(m_stop);
    bar->addAction(m_stopAndInit);

    ref.toolbars.emplace_back(
        bar, StringKey<score::Toolbar>("Transport"), 1, 0);
  }

  {
    auto& play = m_context.menus.get().at(score::Menus::Play());
    play.menu()->addAction(m_play);
    play.menu()->addAction(m_playGlobal);
    play.menu()->addAction(m_stop);
    play.menu()->addAction(m_stopAndInit);
  }

  ref.actions.add<Actions::Play>(m_play);
  ref.actions.add<Actions::PlayGlobal>(m_playGlobal);
  ref.actions.add<Actions::Stop>(m_stop);
  // ref.actions.add<Actions::GoToStart>(m_goToStart);
  // ref.actions.add<Actions::GoToEnd>(m_goToEnd);
  ref.actions.add<Actions::Reinitialize>(m_stopAndInit);
  // ref.actions.add<Actions::Record>(m_record);

  cond.add<Actions::Play>();
  cond.add<Actions::Stop>();
  // cond.add<Actions::GoToStart>();
  // cond.add<Actions::GoToEnd>();
  cond.add<Actions::Reinitialize>();
  // cond.add<Actions::Record>();
}
}
