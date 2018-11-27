#pragma once
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QTableView>

namespace Scenario
{
class MessageItemModel;
class StateModel;
class MessageView final
    : public QTableView
{
public:
  MessageView(const StateModel& model, QWidget* parent);

  MessageItemModel& model() const;
  void removeNodes();

private:
  void resizeEvent(QResizeEvent* ev) override;
  void contextMenuEvent(QContextMenuEvent*) override;

  QAction* m_removeNodesAction{};
  const StateModel& m_model;

  CommandDispatcher<> m_dispatcher;
  float m_valueColumnSize{0.15f};
};
}
