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
#include "IOTypeDelegate.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>


#include "Commands/Add/AddDevice.hpp"
#include "Commands/Add/LoadDevice.hpp"
#include "Commands/Add/AddAddress.hpp"
#include "Commands/Remove.hpp"

#include "ExplorationWorker.hpp"
#include "Commands/ReplaceDevice.hpp"
#include "Commands/UpdateAddresses.hpp"
#include "Commands/Update/UpdateAddressSettings.hpp"
#include "Commands/Update/UpdateDeviceSettings.hpp"
#include "Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"
#include <DeviceExplorer/XML/XMLDeviceLoader.hpp>
#include <QMessageBox>

DeviceExplorerWidget::DeviceExplorerWidget(QWidget* parent)
    : QWidget(parent),
      m_proxyModel(nullptr),
      m_deviceDialog(nullptr), m_addressDialog(nullptr)

{
    buildGUI();
}

void
DeviceExplorerWidget::buildGUI()
{
    m_ntView = new DeviceExplorerView(this);

    m_ntView->setItemDelegateForColumn((int)DeviceExplorerModel::Column::IOType, new IOTypeDelegate);

    connect(m_ntView, SIGNAL(selectionChanged()), this, SLOT(updateActions()));


    /*
    m_cmdQ = new iscore::CommandQueue(this);
    m_undoAction = m_cmdQ->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcuts(QKeySequence::Undo);
    m_redoAction = m_cmdQ->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcuts(QKeySequence::Redo);*/

    m_editAction = new QAction(tr("Edit"), this);
    m_editAction->setShortcut(QKeySequence(Qt::Key_Return));

    m_refreshAction = new QAction(tr("Refresh namespace"), this);
    m_refreshAction->setShortcut(QKeySequence::Refresh);

    m_refreshValueAction = new QAction(tr("Refresh value"), this);

    m_copyAction = new QAction(QIcon(":/resources/images/copy.png"), tr("Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_cutAction = new QAction(QIcon(":/resources/images/cut.png"), tr("Cut"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    m_pasteAction = new QAction(QIcon(":/resources/images/paste.png"), tr("Paste"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_removeNodeAction = new QAction(tr("Remove"), this);
    m_removeNodeAction->setShortcut(QKeySequence::Delete);

    m_moveUpAction = new QAction(QIcon(":/resources/images/up.png"), tr("Move up"), this);
    m_moveUpAction->setShortcut(QKeySequence(tr("Alt+Up")));
    m_moveDownAction = new QAction(QIcon(":/resources/images/down.png"), tr("Move down"), this);
    m_moveDownAction->setShortcut(QKeySequence(tr("Alt+Down")));
    m_promoteAction = new QAction(QIcon(":/resources/images/promote.png"), tr("Promote"), this);
    m_promoteAction->setShortcut(QKeySequence(tr("Alt+Left")));
    m_demoteAction = new QAction(QIcon(":/resources/images/demote.png"), tr("Demote"), this);
    m_demoteAction->setShortcut(QKeySequence(tr("Alt+Right")));

    m_editAction->setEnabled(false);
    m_refreshAction->setEnabled(false);
    m_refreshValueAction->setEnabled(false);
    m_removeNodeAction->setEnabled(false);
    m_copyAction->setEnabled(false);
    m_cutAction->setEnabled(false);
    m_pasteAction->setEnabled(false);
    m_moveUpAction->setEnabled(false);
    m_moveDownAction->setEnabled(false);
    m_promoteAction->setEnabled(false);
    m_demoteAction->setEnabled(false);


    connect(m_editAction, &QAction::triggered, this, &DeviceExplorerWidget::edit);
    connect(m_refreshAction, &QAction::triggered, this, &DeviceExplorerWidget::refresh);
    connect(m_refreshValueAction, &QAction::triggered, this, &DeviceExplorerWidget::refreshValue);
    connect(m_copyAction, &QAction::triggered, this, &DeviceExplorerWidget::copy);
    connect(m_cutAction, &QAction::triggered, this, &DeviceExplorerWidget::cut);
    connect(m_pasteAction, &QAction::triggered, this, &DeviceExplorerWidget::paste);
    connect(m_moveUpAction, &QAction::triggered, this, &DeviceExplorerWidget::moveUp);
    connect(m_moveDownAction, &QAction::triggered, this, &DeviceExplorerWidget::moveDown);
    connect(m_promoteAction, &QAction::triggered, this, &DeviceExplorerWidget::promote);
    connect(m_demoteAction, &QAction::triggered, this, &DeviceExplorerWidget::demote);
    connect(m_removeNodeAction, &QAction::triggered, this, &DeviceExplorerWidget::removeNode);

    /*
    QPushButton *addDeviceButton = new QPushButton(this);
    addDeviceButton->setIcon(QIcon(":/resources/images/addANode.png"));
    addDeviceButton->setToolTip(tr("Add a device..."));
    addDeviceButton->setMaximumSize(QSize(64, 64));
    addDeviceButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    QPushButton *addChildButton = new QPushButton(this);
    addChildButton->setIcon(QIcon(":/resources/images/addChild.png"));
    addChildButton->setToolTip(tr("Add as child..."));
    addChildButton->setMaximumSize(QSize(64, 64));

    QPushButton *addSiblingButton = new QPushButton(this);
    addSiblingButton->setIcon(QIcon(":/resources/images/addSibling.png"));
    addSiblingButton->setToolTip(tr("Add as sibling..."));
    addSiblingButton->setMaximumSize(QSize(64, 64));

    connect(addDeviceButton, SIGNAL(clicked(bool)), this, SLOT(addDevice()));
    connect(addChildButton, SIGNAL(clicked(bool)), this, SLOT(addChild()));
    connect(addSiblingButton, SIGNAL(clicked(bool)), this, SLOT(addSibling()));
    */

    QPushButton* addButton = new QPushButton(this);
    addButton->setIcon(QIcon(":/resources/images/add.png"));
    addButton->setMaximumSize(QSize(32, 32));
    addButton->setStyleSheet("QPushButton::menu-indicator{ image: url(none.jpg); }");  //to hide the small triangle added to indicate a menu.

    m_addDeviceAction = new QAction(QIcon(":/resources/images/addDevice.png"), tr("Add device"), this);
    m_addDeviceAction->setShortcut(tr("Ctrl+B"));
    m_addSiblingAction = new QAction(QIcon(":/resources/images/addSibling.png"), tr("Add sibling"), this);
    m_addChildAction = new QAction(QIcon(":/resources/images/addChild.png"), tr("Add child"), this);

    connect(m_addDeviceAction, SIGNAL(triggered()), this, SLOT(addDevice()));
    connect(m_addSiblingAction, SIGNAL(triggered()), this, SLOT(addSibling()));
    connect(m_addChildAction, SIGNAL(triggered()), this, SLOT(addChild()));

    m_addSiblingAction->setEnabled(false);
    m_addChildAction->setEnabled(false);


    QMenu* addMenu = new QMenu(this);
    addMenu->addAction(m_addDeviceAction);
    addMenu->addAction(m_addSiblingAction);
    addMenu->addAction(m_addChildAction);
    addMenu->addSeparator();
    addMenu->addAction(m_removeNodeAction);

    addButton->setMenu(addMenu);


    QPushButton* editButton = new QPushButton(this);
    editButton->setIcon(QIcon(":/resources/images/edit.png"));
    editButton->setMaximumSize(QSize(32, 32));
    editButton->setStyleSheet("QPushButton::menu-indicator{ image: url(none.jpg); }");  //to hide the small triangle added to indicate a menu.

    QMenu* editMenu = new QMenu(this);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_moveUpAction);
    editMenu->addAction(m_moveDownAction);
    editMenu->addAction(m_promoteAction);
    editMenu->addAction(m_demoteAction);
    editMenu->addSeparator();/*
  editMenu->addAction(m_undoAction);
  editMenu->addAction(m_redoAction);*/

    editButton->setMenu(editMenu);



    m_columnCBox = new QComboBox(this);
    m_nameLEdit = new QLineEdit(this);

    connect(m_columnCBox, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged()));
    connect(m_nameLEdit, SIGNAL(textEdited(const QString&)), this, SLOT(filterChanged()));

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

    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(m_ntView);
    vLayout->addLayout(hLayout);

    setLayout(vLayout);


    installStyleSheet();
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
    QMenu* contextMenu = new QMenu(this);

    connect(contextMenu, &QMenu::triggered,
            [=] () { contextMenu->deleteLater(); });
    contextMenu->addAction(m_editAction);
    contextMenu->addAction(m_refreshAction);
    contextMenu->addAction(m_refreshValueAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_addDeviceAction);
    contextMenu->addAction(m_addSiblingAction);
    contextMenu->addAction(m_addChildAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_copyAction);
    contextMenu->addAction(m_cutAction);
    contextMenu->addAction(m_pasteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_moveUpAction);
    contextMenu->addAction(m_moveDownAction);
    contextMenu->addAction(m_promoteAction);
    contextMenu->addAction(m_demoteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(m_removeNodeAction);/*
  contextMenu->addAction(m_undoAction);
  contextMenu->addAction(m_redoAction);*/

    contextMenu->exec(event->globalPos());
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
                iscore::IDocument::commandStack(*model));

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

        if(selection.isEmpty())
        {
            m_editAction->setEnabled(false);
            m_refreshAction->setEnabled(false);
            m_refreshValueAction->setEnabled(false);
            m_copyAction->setEnabled(false);
            m_cutAction->setEnabled(false);
            m_promoteAction->setEnabled(false);
            m_demoteAction->setEnabled(false);
            m_moveUpAction->setEnabled(false);
            m_moveDownAction->setEnabled(false);
            m_removeNodeAction->setEnabled(false);
        }
        else
        {
            m_refreshAction->setEnabled(true);
            m_refreshValueAction->setEnabled(true);
            m_copyAction->setEnabled(true);
            m_cutAction->setEnabled(true);
//            m_moveUpAction->setEnabled(true);
//            m_moveDownAction->setEnabled(true);
        }

        if(selection.size() == 1)
        {
            const bool aDeviceIsSelected = model()->isDevice(m_ntView->selectedIndex());

            if(! aDeviceIsSelected)
            {
                m_addSiblingAction->setEnabled(true);
                m_promoteAction->setEnabled(true);
                m_demoteAction->setEnabled(true);
                m_removeNodeAction->setEnabled(true);
                m_moveUpAction->setEnabled(true);
                m_moveDownAction->setEnabled(true);
            }
            else
            {
                m_addSiblingAction->setEnabled(false);
                m_promoteAction->setEnabled(false);
                m_demoteAction->setEnabled(false);
                m_removeNodeAction->setEnabled(false);
                m_moveUpAction->setEnabled(false);
                m_moveDownAction->setEnabled(false);
            }
            m_editAction->setEnabled(true);
            m_addChildAction->setEnabled(true);
        }

    }
    else
    {
        m_editAction->setEnabled(false);
        m_refreshAction->setEnabled(false);
        m_refreshValueAction->setEnabled(false);
        m_copyAction->setEnabled(false);
        m_cutAction->setEnabled(false);
        m_promoteAction->setEnabled(false);
        m_demoteAction->setEnabled(false);
        m_moveUpAction->setEnabled(false);
        m_moveDownAction->setEnabled(false);
        m_removeNodeAction->setEnabled(false);
        m_addSiblingAction->setEnabled(false);
        m_addChildAction->setEnabled(false);
    }


    m_pasteAction->setEnabled(model()->hasCut());
}


DeviceExplorerModel*
DeviceExplorerWidget::model()
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
    iscore::Node* select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if (select->is<iscore::DeviceSettings>())
    {
        ISCORE_TODO;
        return;
        /*
        if(! m_deviceDialog)
        {
            m_deviceDialog = new DeviceEditDialog(this);
        }
        auto set = select->get<iscore::DeviceSettings>();
        m_deviceDialog->setSettings(set);

        QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_deviceDialog->exec());

        if(code == QDialog::Accepted)
        {
            auto deviceSettings = m_deviceDialog->getSettings();
            select->setDeviceSettings(deviceSettings);
        }

        updateActions();
        */
    }
    else
    {
        if (! m_addressDialog)
        {
            m_addressDialog = new AddressEditDialog(this);
        }
        auto settings = select->get<iscore::AddressSettings>();
        m_addressDialog->setSettings(settings);

        QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_addressDialog->exec());

        if(code == QDialog::Accepted)
        {
            auto cmd = new DeviceExplorer::Command::UpdateAddressSettings{
                    iscore::IDocument::path(model()->deviceModel()),
                    iscore::NodePath(*select),
                    m_addressDialog->getSettings()};

            m_cmdDispatcher->submitCommand(cmd);
        }

        updateActions();
    }
}

void DeviceExplorerWidget::refresh()
{
    iscore::Node* select = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if ( model()->isDevice(m_ntView->selectedIndex()))
    {
        // Create a thread, ask the device, when it is done put a command on the chain.
        auto& dev = model()->deviceModel().list().device(select->get<iscore::DeviceSettings>().name);
        if(!dev.canRefresh())
            return;

        auto thread = new QThread;
        auto worker = new ExplorationWorker{dev};

        connect(thread, &QThread::started, worker, [=] () {
            try {
                worker->node = worker->dev.refresh();
                emit worker->finished();
            }
            catch(std::runtime_error& e)
            {
                emit worker->failed(e.what());
            }
        }, Qt::QueuedConnection);

        connect(worker, &ExplorationWorker::finished, this,
                [=] () {
            auto cmd = new DeviceExplorer::Command::ReplaceDevice{
                    iscore::IDocument::path(*model()),
                    m_ntView->selectedIndex().row(),
                    std::move(worker->node)};

            m_cmdDispatcher->submitCommand(cmd);

            thread->quit();
            worker->deleteLater();
        }, Qt::QueuedConnection);

        connect(worker, &ExplorationWorker::failed, this,
                [=] (const QString& error_txt) {
            QMessageBox::warning(this,
                                 tr("Unable to refresh the device"),
                                 tr("Unable to refresh the device: ") + select->get<iscore::DeviceSettings>().name + tr(".\nCause: ") + error_txt);
            thread->quit();
            worker->deleteLater();
        });

        worker->moveToThread(thread);
        thread->start();
    }
}

void DeviceExplorerWidget::refreshValue()
{
    QList<QPair<const iscore::Node*, iscore::Value>> lst;
    for(auto& index : m_ntView->selectedIndexes())
    {
        // Model checks
        if (m_ntView->hasProxy())
            index = static_cast<const QAbstractProxyModel *>(m_ntView->QTreeView::model())->mapToSource(index);

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
        lst.append({node, dev.refresh(addr)});
    }

    if(lst.empty())
        return;

    // Send the command
    auto cmd = new DeviceExplorer::Command::UpdateAddresses{
            iscore::IDocument::path(*model()),
            lst};

    m_cmdDispatcher->submitCommand(cmd);
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
        auto path = m_deviceDialog->getPath();

        auto devplug_path = iscore::IDocument::path(model()->deviceModel());
        if(path.isEmpty())
        {
            m_cmdDispatcher->submitCommand(new AddDevice{std::move(devplug_path), deviceSettings});
        }
        else
        {
            iscore::Node n{deviceSettings};
            loadDeviceFromXML(path, n);
            m_cmdDispatcher->submitCommand(new LoadDevice{std::move(devplug_path), std::move(n)});
        }
    }

    updateActions();
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

void DeviceExplorerWidget::removeNode()
{
    iscore::Node* n = model()->nodeFromModelIndex(m_ntView->selectedIndex());
    if(! n->is<iscore::DeviceSettings>())
        m_cmdDispatcher->submitCommand(new DeviceExplorer::Command::Remove{iscore::IDocument::path(model()->deviceModel()), *n});
}

void
DeviceExplorerWidget::addAddress(InsertMode insert)
{
    if(! m_addressDialog)
    {
        m_addressDialog = new AddressEditDialog(this);
    }
    m_addressDialog->setSettings(AddressEditDialog::makeDefaultSettings());

    QDialog::DialogCode code = static_cast<QDialog::DialogCode>(m_addressDialog->exec());

    if(code == QDialog::Accepted)
    {
        const iscore::AddressSettings addressSettings = m_addressDialog->getSettings();
        ISCORE_ASSERT(model());
        QModelIndex index = proxyModel()->mapToSource(m_ntView->currentIndex());
        m_cmdDispatcher->submitCommand(
                    new DeviceExplorer::Command::AddAddress{
                        iscore::IDocument::path(model()->deviceModel()),
                        iscore::NodePath{index},
                        insert, addressSettings });
        updateActions();
    }

}

void
DeviceExplorerWidget::copy()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->copy();
    m_pasteAction->setEnabled(m_ntView->hasCut());   //updateActions(); //TODO???
}

void
DeviceExplorerWidget::cut()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->cut();
    m_pasteAction->setEnabled(m_ntView->hasCut());   //updateActions(); //TODO???
}

void
DeviceExplorerWidget::paste()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->paste();
    m_pasteAction->setEnabled(m_ntView->hasCut());   //updateActions(); //TODO???
}



void
DeviceExplorerWidget::moveUp()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->moveUp();
}

void
DeviceExplorerWidget::moveDown()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->moveDown();
}

void
DeviceExplorerWidget::promote()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->promote();
}

void
DeviceExplorerWidget::demote()
{
    ISCORE_ASSERT(m_ntView);
    m_ntView->demote();
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
