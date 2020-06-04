// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TransportActions.hpp"

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/Menu.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QAction>
#include <QDebug>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QString>
#include <QToolBar>
#include <qnamespace.h>
class QMenu;

namespace Scenario
{
class ScenarioPresenter;

TransportActions::TransportActions(const score::GUIApplicationContext& context)
    : m_context{context}
{
  if (!context.applicationSettings.gui)
    return;
  m_play = new QAction{tr("Play"), nullptr};
  m_play->setCheckable(true);
  m_play->setObjectName("Play");
  m_play->setShortcut(Qt::Key_Space);
  m_play->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_play->setStatusTip("Play the currently displayed interval");
  m_play->setData(false);
  setIcons(
      m_play,
      QStringLiteral(":/icons/play_on.png"),
      QStringLiteral(":/icons/play_hover.png"),
      QStringLiteral(":/icons/play_off.png"),
      QStringLiteral(":/icons/play_disabled.png"));

  m_playGlobal = new QAction{tr("Play Root"), nullptr};
  m_playGlobal->setCheckable(true);
  m_playGlobal->setObjectName("Play Root");
  m_playGlobal->setShortcut(Qt::Key_Shift + Qt::Key_Space);
  m_playGlobal->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_playGlobal->setStatusTip("Play the top-level score from the beginning");
  m_playGlobal->setData(false);
  setIcons(
      m_playGlobal,
      QStringLiteral(":/icons/play_glob_on.png"),
      QStringLiteral(":/icons/play_glob_hover.png"),
      QStringLiteral(":/icons/play_glob_off.png"),
      QStringLiteral(":/icons/play_glob_disabled.png"));

  m_stop = new QAction{tr("Stop"), nullptr};
  m_stop->setObjectName("Stop");
  m_stop->setShortcut(Qt::Key_Return);
  m_stop->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_stop->setStatusTip("Stop execution of the score");
  setIcons(
      m_stop,
      QStringLiteral(":/icons/stop_on.png"),
      QStringLiteral(":/icons/stop_hover.png"),
      QStringLiteral(":/icons/stop_off.png"),
      QStringLiteral(":/icons/stop_disabled.png"));
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
  m_stopAndInit->setStatusTip("Stop execution of the score and send the initial state");

  setIcons(
      m_stopAndInit,
      QStringLiteral(":/icons/reinitialize_on.png"),
      QStringLiteral(":/icons/reinitialize_hover.png"),
      QStringLiteral(":/icons/reinitialize_off.png"),
      QStringLiteral(":/icons/reinitialize_disabled.png"));
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
    m_play->setStatusTip(b ? "Play the currently displayed interval" : "Pause execution");

    setIcons(
        m_play,
        !b ? QStringLiteral(":/icons/play_on.png") : QStringLiteral(":/icons/pause_on.png"),
          !b ? QStringLiteral(":/icons/play_hover.png") : QStringLiteral(":/icons/pause_hover.png"),
        !b ? QStringLiteral(":/icons/play_off.png") : QStringLiteral(":/icons/pause_off.png"),
        !b ? QStringLiteral(":/icons/pause_disabled.png")
          : QStringLiteral(":/icons/pause_disabled.png"));

    m_playGlobal->setText(b ? tr("Pause") : tr("Play (global)"));
    m_playGlobal->setData(b);
    setIcons(
        m_playGlobal,
        !b ? QStringLiteral(":/icons/play_glob_on.png")
          : QStringLiteral(":/icons/pause_on.png"),
        !b ? QStringLiteral(":/icons/play_glob_hover.png")
            : QStringLiteral(":/icons/pause_hover.png"),
        !b ? QStringLiteral(":/icons/play_glob_off.png")
          : QStringLiteral(":/icons/pause_off.png"),
        !b ? QStringLiteral(":/icons/play_glob_disabled.png")
          : QStringLiteral(":/icons/pause_disabled.png"));
  };
  connect(m_play, &QAction::toggled, this, [=](bool b) {
    on_play(b);
    m_playGlobal->setEnabled(false);
  });
  connect(m_playGlobal, &QAction::toggled, this, [=](bool b) {
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
        m_play,
        QStringLiteral(":/icons/play_on.png"),
        QStringLiteral(":/icons/play_hover.png"),
        QStringLiteral(":/icons/play_off.png"),
        QStringLiteral(":/icons/play_disabled.png"));
    m_play->setData(false);

    m_playGlobal->setEnabled(true);
    m_playGlobal->setChecked(false);
    m_playGlobal->setText(tr("Play (global)"));
    setIcons(
        m_playGlobal,
        QStringLiteral(":/icons/play_glob_on.png"),
        QStringLiteral(":/icons/play_glob_hover.png"),
        QStringLiteral(":/icons/play_glob_off.png"),
        QStringLiteral(":/icons/play_glob_disabled.png"));
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

  if (context.documentTabWidget)
  {
    auto obj = context.documentTabWidget;
    obj->addAction(m_play);
    obj->addAction(m_playGlobal);
    obj->addAction(m_stop);
    obj->addAction(m_stopAndInit);
  }
}

TransportActions::~TransportActions() { }

void TransportActions::makeGUIElements(score::GUIElements& ref)
{
  auto& cond = m_context.actions
                   .condition<score::EnableWhenDocumentIs<Scenario::ScenarioDocumentModel>>();

  // Put m_play m_stop and m_stopAndInit only for now in their own toolbar,
  // plus everything in the play menu
  {
    auto bar = new QToolBar{tr("Transport")};
    {
      auto time_label = new QLabel;
      time_label->setObjectName("TimeLabel");
      QFont time_font("Ubuntu", 18, QFont::Weight::DemiBold);
      time_label->setFont(time_font);
      time_label->setText("00:00:00.000");
      bar->addWidget(time_label);
    }

    bar->addAction(m_play);
    bar->addAction(m_playGlobal);
    bar->addAction(m_stop);
    bar->addAction(m_stopAndInit);

    bar->addWidget(new Scenario::SpeedWidget{false, true, bar});

    qDebug() << bar->children();

    ref.toolbars.emplace_back(
        bar, StringKey<score::Toolbar>("Transport"), Qt::BottomToolBarArea, 200);
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
