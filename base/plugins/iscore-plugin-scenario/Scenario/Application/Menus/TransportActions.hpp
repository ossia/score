#pragma once

#include <QList>
#include <QPoint>

#include <iscore/actions/Action.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>

namespace iscore
{
struct GUIApplicationContext;
}
class QAction;
class QMenu;
class QToolBar;

namespace Scenario
{
class TransportActions : public QObject
{
public:
  TransportActions(const iscore::GUIApplicationContext&);

  void makeGUIElements(iscore::GUIElements& ref);

private:
  const iscore::GUIApplicationContext& m_context;

  QAction* m_play{};
  QAction* m_stop{};
  // QAction* m_goToStart{};
  // QAction* m_goToEnd{};
  QAction* m_stopAndInit{};
  // QAction* m_record{};
};
}
