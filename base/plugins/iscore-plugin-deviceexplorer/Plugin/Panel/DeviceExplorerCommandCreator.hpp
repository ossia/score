#pragma once



#include "DeviceExplorerModel.hpp"

#include <QStack>

namespace iscore
{
    class CommandStack;
}
namespace DeviceExplorer {
    namespace Command {
        class Move;
        class Insert;
        class Remove;
        class Copy;
        class Cut;
        class Paste;
    }
}


class DeviceExplorerCommandCreator : public QObject
{
    Q_OBJECT

    friend class DeviceExplorerModel;

    public:
        DeviceExplorerCommandCreator(DeviceExplorerModel* model);
        ~DeviceExplorerCommandCreator();

        void setCommandQueue(iscore::CommandStack* q);

        /*
          The following methods return a QModelIndex that can be used as the new selection.
          change that ?
         */
        QModelIndex copy(const QModelIndex& index);
        QModelIndex cut(const QModelIndex& index);
        QModelIndex paste(const QModelIndex& index);

        //Move up node at @a index among siblings
        QModelIndex moveUp(const QModelIndex& index);

        //Move down node at @a index among siblings
        QModelIndex moveDown(const QModelIndex& index);

        //Move node at @a index at its parent level,
        //i.e., making node a child of its grandparent
        // and the next sibling of its parent.
        QModelIndex promote(const QModelIndex& index);

        //Move node at @a index as child of its up sibling
        QModelIndex demote(const QModelIndex& index);

        void setCachedResult(DeviceExplorer::Result r);

        typedef QPair<iscore::Node*, bool> CutElt;

    protected:
        DeviceExplorer::Result m_cachedResult;

    private:
        DeviceExplorerModel* m_model;
        iscore::CommandStack* m_cmdQ;

};

