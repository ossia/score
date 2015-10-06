#pragma once
#include <QTreeView>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class StateModel;
class DeviceExplorerModel;
// TODO rename file

class MessageItemModel;
class MessageTreeView :  public QTreeView
{
    public:
        MessageTreeView(
                const StateModel& model,
                QWidget* parent);

        MessageItemModel& model() const;

        void removeNodes();

    private:
        //void mouseDoubleClickEvent(QMouseEvent* ev) override;
        void contextMenuEvent(QContextMenuEvent*) override;

        QAction* m_removeNodesAction{};
        StateModel* m_model{};

        CommandDispatcher<> m_dispatcher;
};
