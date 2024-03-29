#pragma once

#include <score/selection/Selection.hpp>

#include <score_plugin_scenario_export.h>
namespace score
{
struct GUIApplicationContext;
struct GUIElements;
}
class QAction;
class QMenu;
class QToolBar;

namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT TransportActions final : public QObject
{
public:
  TransportActions(const score::GUIApplicationContext&);
  ~TransportActions();

  void makeGUIElements(score::GUIElements& ref);

  void onPlayLocal();
  void onPlayGlobal();
  void onPause();
  void onStop();

private:
  void onPlay(bool b);
  const score::GUIApplicationContext& m_context;

  QAction* m_play{};
  QAction* m_playGlobal{};
  QAction* m_stop{};
  // QAction* m_goToStart{};
  // QAction* m_goToEnd{};
  QAction* m_stopAndInit{};
  // QAction* m_record{};
};
}
