#include "DeviceExplorerWidget.hpp"

#include <QAction>
#include <QBoxLayout>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPushButton>
#include <QLineEdit>

#include "Widgets/AddressEditDialog.hpp"
#include "Widgets/DeviceEditDialog.hpp"

#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerView.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>


#include <Explorer/Commands/Add/AddDevice.hpp>
#include <Explorer/Commands/RemoveNodes.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Remove.hpp>

#include "ExplorationWorker.hpp"
#include <Explorer/Commands/ReplaceDevice.hpp>
#include <Explorer/Commands/UpdateAddresses.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include "Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp"
#include <Device/XML/XMLDeviceLoader.hpp>
#include "ExplorationWorkerWrapper.hpp"
#include <Device/Protocol/SingletonProtocolList.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QMessageBox>

#include <QProgressIndicator>


#include <QApplication>



DeviceExplorerWidget::DeviceExplorerWidget(QWidget* parent)
    : QWidget(parent),
      m_proxyModel(nullptr),
      m_deviceDialog(nullptr)
{
    buildGUI();

    // Set the expansion signals
    connect(m_ntView, &QTreeView::expanded,
            this, [&] (const QModelIndex& idx) { setListening(idx, true); });
    connect(m_ntView, &QTreeView::collapsed,
            this,[&] (const QModelIndex& idx) { setListening(idx, false); });
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


    installStyleSheet();
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

QModelIndex DeviceExplorerWidget::sourceIndex(QModelIndex index)
{
    if (m_ntView->hasProxy())
        index = static_cast<const QAbstractProxyModel*>(m_ntView->QTreeView::model())->mapToSource(index);
    return index;
}

void
DeviceExplorerWidget::installStyleSheet()
{
    setStyleSheet(
        "* {"
        // "background-color: #bababa;"
        "}"
    );
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

void DeviceExplorerWidget::setListening_rec(const iscore::Node& node, bool b)
{
    if(node.is<iscore::AddressSettings>())
    {
        auto addr = iscore::address(node);
        auto& dev = model()->deviceModel().list().device(addr.device);
        dev.setListening(addr, b);
    }

    for(const auto& child : node)
    {
        setListening_rec(child, b);
    }
}

void DeviceExplorerWidget::setListening_rec2(const QModelIndex& index, bool b)
{
    const auto& node = model()->nodeFromModelIndex(sourceIndex(index));

    int i = 0;
    for(const auto& child : node)
    {
        if(child.is<iscore::AddressSettings>())
        {
            auto addr = iscore::address(child);
            auto& dev = model()->deviceModel().list().device(addr.device);
            dev.setListening(addr, b);
        }

        // TODO check this
        auto childIndex = index.child(i, 0);

        if(m_ntView->isExpanded(childIndex))
        {
            setListening_rec2(childIndex, b);
        }
        i++;
    }
}


void DeviceExplorerWidget::setListening(const QModelIndex& idx, bool b)
{
    // OPTIMIZEME with the knowledge that a child
    // will have the same device as its parent
    if(b)
    {
        setListening_rec2(idx, b);
    }
    else
    {
        const auto& node = model()->nodeFromModelIndex(sourceIndex(idx));
        for(const auto& child : node)
        {
            setListening_rec(child, false);
        }
    }
}

DeviceExplorerModel*
DeviceExplorerWidget::model() const
{
    return m_ntView->model();
}

DeviceExplorerFilterProxyModel*
DeviceExplorerWidget::proxyModel()
{
    return m_proxyModel;
}

void DeviceExplorerWidget::edit()
{
    const auto& select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select.is<iscore::DeviceSettings>())
    {
        if(! m_deviceDialog)
        {
            m_deviceDialog = new DeviceEditDialog(this);
        }
        auto set = select.get<iscore::DeviceSettings>();
        m_deviceDialog->setSettings(set);

        QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

        if(code == QDialog::Accepted)
        {
            auto cmd = new DeviceExplorer::Command::UpdateDeviceSettings{
                    model()->deviceModel(),
                    set.name,
                    m_deviceDialog->getSettings()};

            m_cmdDispatcher->submitCommand(cmd);
        }

        updateActions();
    }
    else
    {
        auto before = select.get<iscore::AddressSettings>();
        AddressEditDialog dial{before, this};

        auto code = static_cast<QDialog::DialogCode>(dial.exec());

        if(code == QDialog::Accepted)
        {
            auto stgs = dial.getSettings();
            // TODO do like for DeviceSettings
            if(!model()->checkAddressEditable(*select.parent(), before, stgs))
                return;

            auto cmd = new DeviceExplorer::Command::UpdateAddressSettings{
                    model()->deviceModel(),
                    iscore::NodePath(select),
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
    if (select.is<iscore::DeviceSettings>())
    {
        // Create a thread, ask the device, when it is done put a command on the chain.
        auto& dev = m->deviceModel().list().device(select.get<iscore::DeviceSettings>().name);
        if(!dev.canRefresh())
            return;

        auto wrkr = make_worker(
            [=] (iscore::Node&& node) {
                auto cmd = new DeviceExplorer::Command::ReplaceDevice{
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
    QList<QPair<const iscore::Node*, iscore::Value>> lst;
    for(auto index : m_ntView->selectedIndexes())
    {
        // Model checks
        index = sourceIndex(index);
        iscore::Node* node = index.isValid()
                              ? static_cast<iscore::Node*>(index.internalPointer())
                              : nullptr;

        if(!node || node->is<iscore::DeviceSettings>())
            continue;

        // Device checks
        auto addr = iscore::address(*node);
        auto& dev = model()->deviceModel().list().device(addr.device);
        if(!dev.canRefresh())
            return;

        // Getting the new values
        auto val = dev.refresh(addr);
        if(val)
            lst.append({node, *val});
    }

    if(lst.empty())
        return;

    // Send the command
    auto cmd = new DeviceExplorer::Command::UpdateAddressesValues{
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
    if (select.is<iscore::DeviceSettings>())
    {
        auto& dev = m->deviceModel().list().device(select.get<iscore::DeviceSettings>().name);
        dev.disconnect();
    }
}

void DeviceExplorerWidget::reconnect()
{
    auto m = model();
    if(!m)
        return;

    const auto& select = m->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select.is<iscore::DeviceSettings>())
    {
        auto& dev = m->deviceModel().list().device(select.get<iscore::DeviceSettings>().name);
        dev.reconnect();
    }
}

void
DeviceExplorerWidget::addDevice()
{
    if(! m_deviceDialog)
    {
        m_deviceDialog = new DeviceEditDialog(this);
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
            m_cmdDispatcher->submitCommand(new AddDevice{std::move(devplug_path), deviceSettings});
        }
        else
        {
            iscore::Node n{deviceSettings, nullptr};
            loadDeviceFromXML(path, n);
            m_cmdDispatcher->submitCommand(new LoadDevice{std::move(devplug_path), std::move(n)});
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

    QList<iscore::Node*> nodes;
    for(auto index : indexes)
    {
        auto& n = model()->nodeFromModelIndex(sourceIndex(index));
        if(!n.is<InvisibleRootNodeTag>())
            nodes.append(&n);
    }

    auto cmd = new RemoveNodes;
    auto dev_model_path = iscore::IDocument::path(model()->deviceModel());
    for(const auto& n : iscore::filterUniqueParents(nodes))
    {
        cmd->addCommand(new DeviceExplorer::Command::Remove{
                            dev_model_path,
                            *n});
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
        if(insert == InsertMode::AsSibling && node.is<iscore::DeviceSettings>())
        {
            return;
        }

        iscore::Node* parent =
              (insert == InsertMode::AsChild)
                ? &node
                : node.parent();

        auto stgs = dial.getSettings();
        if(!model()->checkAddressInstantiatable(*parent, stgs))
            return;

        m_cmdDispatcher->submitCommand(
                        new DeviceExplorer::Command::AddAddress{
                            model()->deviceModel(),
                            iscore::NodePath{index},
                            insert,
                            stgs});
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
    //See http://qt-project.org/doc/qt-5/qregexp.html#PatternSyntax-enum

    QRegExp regExp(pattern, cs, syntax);

    m_proxyModel->setFilterRegExp(regExp);

}
