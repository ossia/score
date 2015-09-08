#pragma once

#include <QWidget>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <QStackedLayout>
#include <QThread>
#include "DeviceExplorerModel.hpp"
class QProgressIndicator;
class DeviceExplorerView;
class DeviceExplorerModel;
class DeviceExplorerFilterProxyModel;
class DeviceEditDialog;
class AddressEditDialog;
namespace iscore
{
    class CommandStack;
}

class QComboBox;
class QLineEdit;


class DeviceExplorerWidget : public QWidget
{
        Q_OBJECT

    public:
        explicit DeviceExplorerWidget(QWidget* parent);

        void setModel(DeviceExplorerModel* model);

        // Will block the GUI when refreshing.
        void blockGUI(bool);

    private:
        // User commands
        void edit();
        void refresh();
        void refreshValue();
        void copy();
        void cut();
        void paste();

        void moveUp();
        void moveDown();
        void promote();
        void demote();


        void addAddress(InsertMode insertType);
        void addDevice();
        void addChild();
        void addSibling();

        void removeNode();

        // Answer to user interaction
        void filterChanged();

        void updateActions();


        // Utilities
        DeviceExplorerModel* model();
        DeviceExplorerFilterProxyModel* proxyModel();

        void buildGUI();
        void installStyleSheet();
        void populateColumnCBox();

        virtual void contextMenuEvent(QContextMenuEvent* event) override;

        DeviceExplorerView* m_ntView{};
        DeviceExplorerFilterProxyModel* m_proxyModel{};
        DeviceEditDialog* m_deviceDialog{};

        QAction* m_editAction{};
        QAction* m_refreshAction{};
        QAction* m_refreshValueAction{};
        QAction* m_addDeviceAction{};
        QAction* m_addSiblingAction{};
        QAction* m_addChildAction{};

        QAction* m_copyAction{};
        QAction* m_cutAction{};
        QAction* m_pasteAction{};
        QAction* m_moveUpAction{};
        QAction* m_moveDownAction{};
        QAction* m_promoteAction{};
        QAction* m_demoteAction{};

        QAction* m_removeNodeAction{};

        QComboBox* m_columnCBox{};
        QLineEdit* m_nameLEdit{};

        std::unique_ptr<CommandDispatcher<>> m_cmdDispatcher;

        QProgressIndicator* m_refreshIndicator{};
        QStackedLayout* m_lay{};
};

