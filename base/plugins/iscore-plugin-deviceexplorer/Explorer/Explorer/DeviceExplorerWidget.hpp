#pragma once

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <QAbstractItemModel>
#include <QWidget>
#include <memory>

#include <Device/Node/DeviceNode.hpp>
#include <iscore/tools/TreePath.hpp>

class DeviceEditDialog;
class DeviceExplorerFilterProxyModel;
class DeviceExplorerModel;
class DeviceExplorerView;
class DynamicProtocolList;
class QAction;
class QContextMenuEvent;
class QProgressIndicator;
class QStackedLayout;

namespace iscore
{
}

class QComboBox;
class QLineEdit;


class DeviceExplorerWidget final : public QWidget
{
        Q_OBJECT

    public:
        explicit DeviceExplorerWidget(
                const DynamicProtocolList&,
                QWidget* parent);

        void setModel(DeviceExplorerModel* model);
        DeviceExplorerModel* model() const;

        // Will block the GUI when refreshing.
        void blockGUI(bool);

    private:
        QModelIndex sourceIndex(QModelIndex);
        // User commands
        void edit();
        void refresh();
        void refreshValue();

        void disconnect();
        void reconnect();

        void addAddress(InsertMode insertType);
        void addDevice();
        void addChild();
        void addSibling();

        void removeNodes();

        // Answer to user interaction
        void filterChanged();

        void updateActions();

        void setListening(const QModelIndex&, bool);

        /**
         * @brief setListening_rec Simple recursion to enable / disable listening
         * on all child nodes.
         */
        void setListening_rec(const Device::Node& node, bool b);

        /**
         * @brief setListening_rec2 Recursion that checks the "expanded" state
         * of nodes.
         */
        void setListening_rec2(const QModelIndex& index, bool b);


        // Utilities
        DeviceExplorerFilterProxyModel* proxyModel();

        void buildGUI();
        void installStyleSheet();
        void populateColumnCBox();

        void contextMenuEvent(QContextMenuEvent* event) override;


        const DynamicProtocolList& m_protocolList;

        DeviceExplorerView* m_ntView{};
        DeviceExplorerFilterProxyModel* m_proxyModel{};
        DeviceEditDialog* m_deviceDialog{};

        QAction* m_disconnect{};
        QAction* m_reconnect{};

        QAction* m_editAction{};
        QAction* m_refreshAction{};
        QAction* m_refreshValueAction{};
        QAction* m_addDeviceAction{};
        QAction* m_addSiblingAction{};
        QAction* m_addChildAction{};

        QAction* m_removeNodeAction{};

        QComboBox* m_columnCBox{};
        QLineEdit* m_nameLEdit{};

        std::unique_ptr<CommandDispatcher<>> m_cmdDispatcher;

        QProgressIndicator* m_refreshIndicator{};
        QStackedLayout* m_lay{};
};

