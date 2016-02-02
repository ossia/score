#include <Device/XML/XMLDeviceLoader.hpp>
#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/AddDevice.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/Commands/RemoveNodes.hpp>
#include <Explorer/Commands/ReplaceDevice.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/Commands/UpdateAddresses.hpp>

#include <boost/optional/optional.hpp>
#include <QAbstractProxyModel>
#include <QAction>
#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QGridLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QContextMenuEvent>
#include <qnamespace.h>
#include <set>
#include <QPair>
#include <QPushButton>
#include <QRegExp>
#include <QSet>
#include <QSize>
#include <QStackedLayout>
#include <QString>
#include <QStringList>
#include <QTreeView>
#include <algorithm>
#include <stdexcept>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerView.hpp"
#include "DeviceExplorerWidget.hpp"
#include "ExplorationWorkerWrapper.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include "QProgressIndicator.h"
#include <State/Address.hpp>
#include <State/Value.hpp>
#include "Widgets/AddressEditDialog.hpp"
#include "Widgets/DeviceEditDialog.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/InvisibleRootNode.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/std/Algorithms.hpp>


namespace Explorer
{
DeviceExplorerWidget::DeviceExplorerWidget(
        const Device::DynamicProtocolList& pl,
        QWidget* parent)
    : QWidget(parent),
      m_protocolList{pl},
      m_proxyModel(nullptr),
      m_deviceDialog(nullptr),
      m_listeningManager{*this}
{
    buildGUI();

    // Set the expansion signals
    connect(m_ntView, &QTreeView::expanded,
            this, [&] (const QModelIndex& idx) { m_listeningManager.setListening(idx, true); });
    connect(m_ntView, &QTreeView::collapsed,
            this,[&] (const QModelIndex& idx) { m_listeningManager.setListening(idx, false); });
}

void
DeviceExplorerWidget::buildGUI()
{
    m_ntView = new DeviceExplorerView(this);

    connect(m_ntView, static_cast<void (DeviceExplorerView::*)()>(&DeviceExplorerView::selectionChanged),
            this, &DeviceExplorerWidget::updateActions);

    m_editAction = new QAction(tr("Edit"), this);
    m_editAction->setShortcut(QKeySequence(Qt::Key_Return));

    m_refreshAction = new QAction(tr("Refresh namespace"), this);
    m_refreshAction->setShortcut(QKeySequence::Refresh);

    m_disconnect = new QAction{tr("Disconnect"), this};
    m_reconnect = new QAction{tr("Reconnect"), this};

    m_refreshValueAction = new QAction(tr("Refresh value"), this);

    m_removeNodeAction = new QAction(tr("Remove"), this);
#ifdef __APPLE__
    m_removeNodeAction->setShortcut(QKeySequence(tr("Ctrl+Backspace")));
#else
    m_removeNodeAction->setShortcut(QKeySequence::Delete);
#endif

    m_editAction->setEnabled(false);
    m_refreshAction->setEnabled(false);
    m_refreshValueAction->setEnabled(false);
    m_removeNodeAction->setEnabled(false);
    m_disconnect->setEnabled(false);
    m_reconnect->setEnabled(false);

    m_editAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_refreshAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_refreshValueAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_removeNodeAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_disconnect->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_reconnect->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    connect(m_editAction, &QAction::triggered, this, &DeviceExplorerWidget::edit);
    connect(m_refreshAction, &QAction::triggered, this, &DeviceExplorerWidget::refresh);
    connect(m_refreshValueAction, &QAction::triggered, this, &DeviceExplorerWidget::refreshValue);
    connect(m_disconnect, &QAction::triggered, this, &DeviceExplorerWidget::disconnect);
    connect(m_reconnect, &QAction::triggered, this, &DeviceExplorerWidget::reconnect);
    connect(m_removeNodeAction, &QAction::triggered, this, &DeviceExplorerWidget::removeNodes);


    QPushButton* addButton = new QPushButton(this);
    addButton->setIcon(QIcon(":/resources/images/add.png"));
    addButton->setMaximumSize(QSize(32, 32));
    addButton->setStyleSheet("QPushButton::menu-indicator{ image: url(none.jpg); }");  //to hide the small triangle added to indicate a menu.

    m_addDeviceAction = new QAction(QIcon(":/resources/images/addDevice.png"), tr("Add device"), this);
    m_addDeviceAction->setShortcut(tr("Ctrl+B"));
    m_addSiblingAction = new QAction(QIcon(":/resources/images/addSibling.png"), tr("Add sibling"), this);
    m_addChildAction = new QAction(QIcon(":/resources/images/addChild.png"), tr("Add child"), this);

    connect(m_addDeviceAction, &QAction::triggered, this, &DeviceExplorerWidget::addDevice);
    connect(m_addSiblingAction, &QAction::triggered, this, &DeviceExplorerWidget::addSibling);
    connect(m_addChildAction, &QAction::triggered, this, &DeviceExplorerWidget::addChild);

    m_addSiblingAction->setEnabled(false);
    m_addChildAction->setEnabled(false);

    // Setup menus

    QMenu* addMenu = new QMenu(this);
    addMenu->addAction(m_addDeviceAction);
    addMenu->addAction(m_addSiblingAction);
    addMenu->addAction(m_addChildAction);
    addMenu->addSeparator();
    addMenu->addAction(m_removeNodeAction);

    addButton->setMenu(addMenu);

    QMenu* editMenu = new QMenu(this);
    QPushButton* editButton = new QPushButton(this);
    editButton->setIcon(QIcon(":/resources/images/edit.png"));
    editButton->setMaximumSize(QSize(32, 32));
    editButton->setStyleSheet("QPushButton::menu-indicator{ image: url(none.jpg); }");  //to hide the small triangle added to indicate a menu.

    editButton->setMenu(editMenu);

    // Add actions to the current widget so that shortcuts work
    {
        this->addAction(m_addDeviceAction);
        this->addAction(m_addSiblingAction);
        this->addAction(m_addChildAction);

        this->addAction(m_refreshAction);
        this->addAction(m_refreshValueAction);

        this->addAction(m_removeNodeAction);
    }


    m_columnCBox = new QComboBox(this);
    m_nameLEdit = new QLineEdit(this);

    connect(m_columnCBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &DeviceExplorerWidget::filterChanged);
    connect(m_nameLEdit, &QLineEdit::textEdited,
            this, &DeviceExplorerWidget::filterChanged);

    QHBoxLayout* filterHLayout = new QHBoxLayout;
    filterHLayout->setContentsMargins(0, 0, 0, 0);
    filterHLayout->addWidget(m_columnCBox);
    filterHLayout->addWidget(m_nameLEdit);




    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addWidget(addButton);
    hLayout->addWidget(editButton);
    //hLayout->addStretch(0);
    hLayout->addLayout(filterHLayout);
    hLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* mainWidg = new QWidget;
    mainWidg->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(m_ntView);
    vLayout->addLayout(hLayout);
    mainWidg->setLayout(vLayout);

    m_lay = new QStackedLayout;
    m_lay->addWidget(mainWidg);

    auto refreshParent = new QWidget;
    auto refreshLay = new QGridLayout;
    refreshParent->setLayout(refreshLay);
    m_refreshIndicator = new QProgressIndicator{refreshParent};
    m_refreshIndicator->setStyleSheet("background:transparent");
    m_refreshIndicator->setAttribute(Qt::WA_TranslucentBackground);
    refreshLay->addWidget(m_refreshIndicator);
    m_lay->addWidget(refreshParent);
    setLayout(m_lay);
}

void DeviceExplorerWidget::blockGUI(bool b)
{
    m_ntView->setDisabled(b);
    if(b)
    {
        // m_ntView to front
        m_lay->setCurrentIndex(1);
        m_refreshIndicator->startAnimation();
    }
    else
    {
        // progreess widget to front
        m_lay->setCurrentIndex(0);
        m_refreshIndicator->stopAnimation();
    }
}

QModelIndex DeviceExplorerWidget::sourceIndex(QModelIndex index) const
{
    if (m_ntView->hasProxy())
        index = static_cast<const QAbstractProxyModel*>(m_ntView->QTreeView::model())->mapToSource(index);
    return index;
}

QModelIndex DeviceExplorerWidget::proxyIndex(QModelIndex index) const
{
    if (m_ntView->hasProxy())
        index = static_cast<const QAbstractProxyModel*>(m_ntView->QTreeView::model())->mapFromSource(index);
    return index;

}

void
DeviceExplorerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    updateActions();
    QMenu contextMenu{this};

    contextMenu.addAction(m_editAction);
    contextMenu.addAction(m_refreshAction);
    contextMenu.addAction(m_refreshValueAction);

    contextMenu.addAction(m_disconnect);
    contextMenu.addAction(m_reconnect);

    contextMenu.addSeparator();
    contextMenu.addAction(m_addDeviceAction);
    contextMenu.addAction(m_addSiblingAction);
    contextMenu.addAction(m_addChildAction);
    contextMenu.addSeparator();
    contextMenu.addAction(m_removeNodeAction);

    contextMenu.exec(event->globalPos());
}

void
DeviceExplorerWidget::setModel(DeviceExplorerModel* model)
{
    delete m_proxyModel;    //? will also delete previous model ??
    m_proxyModel = nullptr;

    if (model)
    {
        m_proxyModel = new DeviceExplorerFilterProxyModel(this);
        m_proxyModel->setSourceModel(model);
        m_ntView->setModel(m_proxyModel);
        model->setView(m_ntView);

        m_cmdDispatcher = std::make_unique<CommandDispatcher<>>(
                model->commandStack());

        populateColumnCBox();

        updateActions();
    }
    else
    {
        m_ntView->setModel((QAbstractItemModel*)nullptr);
    }
}

void
DeviceExplorerWidget::populateColumnCBox()
{
    ISCORE_ASSERT(model());
    ISCORE_ASSERT(m_columnCBox);

    QStringList columns = model()->getColumns();
    m_columnCBox->clear();
    m_columnCBox->addItems(columns);
}

void
DeviceExplorerWidget::updateActions()
{
    ISCORE_ASSERT(model());

    if(! model()->isEmpty())
    {

        //TODO: choice for multi selection

        ISCORE_ASSERT(m_ntView);

        QModelIndexList selection = m_ntView->selectedIndexes();

        m_addSiblingAction->setEnabled(false);
        m_addChildAction->setEnabled(false);

        m_reconnect->setEnabled(false);
        m_disconnect->setEnabled(false);

        if(selection.isEmpty())
        {
            m_editAction->setEnabled(false);
            m_refreshAction->setEnabled(false);
            m_refreshValueAction->setEnabled(false);
            m_removeNodeAction->setEnabled(false);
        }
        else
        {
            m_refreshAction->setEnabled(true);
            m_refreshValueAction->setEnabled(true);
        }

        if(selection.size() == 1)
        {
            const bool aDeviceIsSelected = model()->isDevice(m_ntView->selectedIndex());

            if(! aDeviceIsSelected)
            {
                m_addSiblingAction->setEnabled(true);
            }
            else
            {
                m_reconnect->setEnabled(true);
                m_disconnect->setEnabled(true);
                m_addSiblingAction->setEnabled(false);
                m_removeNodeAction->setEnabled(false);
            }
            m_removeNodeAction->setEnabled(true);
            m_editAction->setEnabled(true);
            m_addChildAction->setEnabled(true);
        }

    }
    else
    {
        m_editAction->setEnabled(false);
        m_refreshAction->setEnabled(false);
        m_refreshValueAction->setEnabled(false);
        m_removeNodeAction->setEnabled(false);
        m_addSiblingAction->setEnabled(false);
        m_addChildAction->setEnabled(false);
    }
}

DeviceExplorerModel*
DeviceExplorerWidget::model() const
{
    return m_ntView->model();
}

DeviceExplorerView* DeviceExplorerWidget::view() const
{
    return m_ntView;
}

DeviceExplorerFilterProxyModel*
DeviceExplorerWidget::proxyModel()
{
    return m_proxyModel;
}

void DeviceExplorerWidget::edit()
{
    const auto& select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select.is<Device::DeviceSettings>())
    {
        if(! m_deviceDialog)
        {
            m_deviceDialog = new DeviceEditDialog{m_protocolList, this};
        }
        auto set = select.get<Device::DeviceSettings>();
        m_deviceDialog->setSettings(set);

        QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

        if(code == QDialog::Accepted)
        {
            auto cmd = new Explorer::Command::UpdateDeviceSettings{
                    model()->deviceModel(),
                    set.name,
                    m_deviceDialog->getSettings()};

            m_cmdDispatcher->submitCommand(cmd);
        }

        updateActions();
    }
    else
    {
        auto before = select.get<Device::AddressSettings>();
        AddressEditDialog dial{before, this};

        auto code = static_cast<QDialog::DialogCode>(dial.exec());

        if(code == QDialog::Accepted)
        {
            auto stgs = dial.getSettings();
            // TODO do like for DeviceSettings
            if(!model()->checkAddressEditable(*select.parent(), before, stgs))
                return;

            auto cmd = new Explorer::Command::UpdateAddressSettings{
                    model()->deviceModel(),
                    Device::NodePath(select),
                    stgs};

            m_cmdDispatcher->submitCommand(cmd);
        }

        updateActions();
    }
}

void DeviceExplorerWidget::refresh()
{
    auto m = model();
    if(!m)
        return;

    const auto& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select.is<Device::DeviceSettings>())
    {
        // Create a thread, ask the device, when it is done put a command on the chain.
        auto& dev = m->deviceModel().list().device(select.get<Device::DeviceSettings>().name);
        if(!dev.capabilities().canRefresh)
            return;

        auto wrkr = make_worker(
            [=] (Device::Node&& node) {
                auto cmd = new Explorer::Command::ReplaceDevice{
                    *m,
                    m_ntView->selectedIndex().row(),
                    std::move(node)};

                m_cmdDispatcher->submitCommand(cmd);
        }, *this, dev);

        wrkr->start();
    }
}

void DeviceExplorerWidget::refreshValue()
{
    // TODO deprecate this
    QList<QPair<const Device::Node*, State::Value>> lst;
    for(auto index : m_ntView->selectedIndexes())
    {
        // Model checks
        index = sourceIndex(index);
        Device::Node* node = index.isValid()
                              ? static_cast<Device::Node*>(index.internalPointer())
                              : nullptr;

        if(!node || node->is<Device::DeviceSettings>())
            continue;

        // Device checks
        auto addr = Device::address(*node);
        auto& dev = model()->deviceModel().list().device(addr.device);
        if(!dev.capabilities().canRefresh)
            return;

        // Getting the new values
        auto val = dev.refresh(addr);
        if(val)
            lst.append({node, *val});
    }

    if(lst.empty())
        return;

    // Send the command
    auto cmd = new Explorer::Command::UpdateAddressesValues{
            *model(),
            lst};

    m_cmdDispatcher->submitCommand(cmd);
}

void DeviceExplorerWidget::disconnect()
{
    auto m = model();
    if(!m)
        return;

    const auto& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select.is<Device::DeviceSettings>())
    {
        auto& dev = m->deviceModel().list().device(select.get<Device::DeviceSettings>().name);
        dev.disconnect();
    }
}

void DeviceExplorerWidget::reconnect()
{
    auto m = model();
    if(!m)
        return;

    const auto& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select.is<Device::DeviceSettings>())
    {
        auto& dev = m->deviceModel().list().device(select.get<Device::DeviceSettings>().name);
        dev.reconnect();
    }
}

void
DeviceExplorerWidget::addDevice()
{
    if(! m_deviceDialog)
    {
        m_deviceDialog = new DeviceEditDialog{m_protocolList, this};
    }

    QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

    if(code == QDialog::Accepted)
    {
        ISCORE_ASSERT(model());
        auto deviceSettings = m_deviceDialog->getSettings();
        if(!model()->checkDeviceInstantiatable(deviceSettings))
        {
            if(!model()->tryDeviceInstantiation(deviceSettings, *m_deviceDialog))
            {
                delete m_deviceDialog;
                m_deviceDialog = nullptr;
                return;
            }
        }

        auto path = m_deviceDialog->getPath();
        blockGUI(true);

        auto devplug_path = iscore::IDocument::path(model()->deviceModel());
        if(path.isEmpty())
        {
            m_cmdDispatcher->submitCommand(new Command::AddDevice{std::move(devplug_path), deviceSettings});
        }
        else
        {
            Device::Node n{deviceSettings, nullptr};
            loadDeviceFromXML(path, n);
            m_cmdDispatcher->submitCommand(new Command::LoadDevice{std::move(devplug_path), std::move(n)});
        }

        blockGUI(false);
    }

    updateActions();
    delete m_deviceDialog;
    m_deviceDialog = nullptr;
}

void
DeviceExplorerWidget::addChild()
{
    addAddress(InsertMode::AsChild);
}

void
DeviceExplorerWidget::addSibling()
{
    addAddress(InsertMode::AsSibling);
}

void DeviceExplorerWidget::removeNodes()
{
    auto indexes = m_ntView->selectedIndexes();

    QList<Device::Node*> nodes;
    for(auto index : indexes)
    {
        auto& n = model()->nodeFromModelIndex(sourceIndex(index));
        if(!n.is<InvisibleRootNodeTag>())
            nodes.append(&n);
    }

    auto cmd = new Command::RemoveNodes;
    auto dev_model_path = iscore::IDocument::path(model()->deviceModel());

    // If two nodes have the same parent,
    // we should send the commands in reverse order
    // (from the last to the first)
    // so that they are emplaced in correct order afterwards.
    // IMPORTANT ! don't use emplace, only emplace_back in D.E. model
    struct PathComparator
    {
            bool operator() (const Device::NodePath& lhs, const Device::NodePath& rhs) const
            {
                // We iterate on the shorter.
                // The shorter is considered "smaller" : it comes before.
                int l_size = lhs.size();
                int r_size = rhs.size();
                if(l_size < r_size)
                {
                    // lhs shorter
                    for(int i = 0; i < l_size; i++)
                    {
                        if(lhs[i] < rhs[i])
                            return true;
                        else if(lhs[i] == rhs[i])
                            continue;
                        else if(lhs[i] > rhs[i])
                            return false;
                    }
                    return true;
                }
                else if(l_size == r_size)
                {
                    for(int i = 0; i < l_size; i++)
                    {
                        if(lhs[i] < rhs[i])
                            return true;
                        else if(lhs[i] == rhs[i])
                            continue;
                        else if(lhs[i] > rhs[i])
                            return false;
                    }
                    ISCORE_ABORT;
                }
                else
                {
                    // rhs shorter
                    for(int i = 0; i < r_size; i++)
                    {
                        if(lhs[i] < rhs[i])
                            return true;
                        else if(lhs[i] == rhs[i])
                            continue;
                        else if(lhs[i] > rhs[i])
                            return false;
                    }
                    return false;
                }

                ISCORE_ABORT;
            }
    };

    std::set<Device::NodePath, PathComparator> paths;
    for(const auto& n : Device::filterUniqueParents(nodes))
    {
        if (n->is<Device::DeviceSettings>())
        {
            cmd->addCommand(new Explorer::Command::Remove{
                                dev_model_path,
                                *n});
        }
        else
        {
            paths.insert(*n);
        }
    }
/*
    for(auto path : paths)
    {
        qDebug() << path;
    }
*/
    for(auto it = paths.rbegin(); it != paths.rend(); ++it)
    {
        cmd->addCommand(
                    new Explorer::Command::Remove{
                        dev_model_path,
                        Device::NodePath{*it}});
    }

    m_cmdDispatcher->submitCommand(cmd);
}

void
DeviceExplorerWidget::addAddress(InsertMode insert)
{
    AddressEditDialog dial{this};
    auto code = static_cast<QDialog::DialogCode>(dial.exec());

    if(code == QDialog::Accepted)
    {
        ISCORE_ASSERT(model());
        QModelIndex index = proxyModel()->mapToSource(m_ntView->currentIndex());

        // If the node is added in sibling mode, we check that no sibling have
        // the same name
        // Else we check that no child of the index has the same name.
        auto& node = model()->nodeFromModelIndex(index);

        // TODO not very elegant.
        if(insert == InsertMode::AsSibling && node.is<Device::DeviceSettings>())
        {
            return;
        }

        Device::Node* parent =
              (insert == InsertMode::AsChild)
                ? &node
                : node.parent();

        auto stgs = dial.getSettings();
        if(!model()->checkAddressInstantiatable(*parent, stgs))
            return;

        bool parent_is_expanded = m_ntView->isExpanded(proxyIndex(m_ntView->model()->modelIndexFromNode(*parent, 0)));

        m_cmdDispatcher->submitCommand(
                        new Explorer::Command::AddAddress{
                            model()->deviceModel(),
                            Device::NodePath{index},
                            insert,
                            stgs});


        // If the node is going to be visible, we have to start listening to it.
        if(parent_is_expanded)
        {
            auto child_it = find_if(*parent, [&] (const auto& child) {
                return child.template get<Device::AddressSettings>().name == stgs.name;
            });
            ISCORE_ASSERT(child_it != parent->end());

            m_listeningManager.enableListening(*child_it);
        }
        updateActions();
    }
}

void
DeviceExplorerWidget::filterChanged()
{
    ISCORE_ASSERT(m_proxyModel);
    ISCORE_ASSERT(m_nameLEdit);

    QString pattern = m_nameLEdit->text();
    Qt::CaseSensitivity cs = Qt::CaseSensitive;

    QRegExp::PatternSyntax syntax = QRegExp::WildcardUnix; //RegExp; //Wildcard; //WildcardUnix; //?
    //See http://qt-project.org/doc/qt-5/QRegExptml#PatternSyntax-enum

    QRegExp regExp(pattern, cs, syntax);

    m_proxyModel->setFilterRegExp(regExp);
    m_proxyModel->setColumn((Explorer::Column)m_columnCBox->currentIndex());
}
}
