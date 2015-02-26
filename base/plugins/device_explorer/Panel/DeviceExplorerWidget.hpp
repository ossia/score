#pragma once

#include <QWidget>

class DeviceExplorerView;
class DeviceExplorerModel;
class DeviceExplorerFilterProxyModel;
class DeviceEditDialog;
class AddressEditDialog;
namespace iscore
{
    class CommandQueue;
}

class QComboBox;
class QLineEdit;


class DeviceExplorerWidget : public QWidget
{
        Q_OBJECT

    public:
        DeviceExplorerWidget (QWidget* parent);

        void setModel (DeviceExplorerModel* model);

        bool loadModel (const QString filename);

    public slots:
        void copy();
        void cut();
        void paste();

        void moveUp();
        void moveDown();
        void promote();
        void demote();


    protected slots:
        void addDevice();
        void addChild();
        void addSibling();

        void filterChanged();

        void updateActions();

    protected:

        DeviceExplorerModel* model();
        DeviceExplorerFilterProxyModel* proxyModel();

        void buildGUI();
        void installStyleSheet();
        void populateColumnCBox();

        void addAddress (int insertType);

        virtual void contextMenuEvent (QContextMenuEvent* event) override;

    protected:

        DeviceExplorerView* m_ntView;
        DeviceExplorerFilterProxyModel* m_proxyModel;
        DeviceEditDialog* m_deviceDialog;
        AddressEditDialog* m_addressDialog;
        //iscore::CommandQueue *m_cmdQ;
//  QAction *m_undoAction;
//  QAction *m_redoAction;

        QAction* m_addDeviceAction;
        QAction* m_addSiblingAction;
        QAction* m_addChildAction;

        QAction* m_copyAction;
        QAction* m_cutAction;
        QAction* m_pasteAction;
        QAction* m_moveUpAction;
        QAction* m_moveDownAction;
        QAction* m_promoteAction;
        QAction* m_demoteAction;

        QComboBox* m_columnCBox;
        QLineEdit* m_nameLEdit;

};

